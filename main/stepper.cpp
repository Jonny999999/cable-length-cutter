//custom driver for stepper motor

#include "config.hpp"
#include "hal/timer_types.h"

#include <cstdint>
#include <inttypes.h>

//config from config.hpp
//#define STEPPER_STEP_PIN GPIO_NUM_18    //mos1
//#define STEPPER_DIR_PIN GPIO_NUM_16     //ST3
//

extern "C" {
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_log.h"
}

#define TIMER_F 1000000ULL
#define TICK_PER_S TIMER_S
#define NS_TO_T_TICKS(x) (x)

static const char *TAG = "stepper-ctl"; //tag for logging
bool direction = 1;
bool timerIsRunning = false;

bool timer_isr(void *arg);


	static timer_group_t timerGroup = TIMER_GROUP_0;
	static timer_idx_t timerIdx = TIMER_0;
	static uint64_t posTarget = 0;
	static uint64_t posNow = 0;


//stepper driver in software for testing (no timer/interrupt):
uint64_t stepsTarget = 0;
void task_stepperCtrlSw(void *pvParameter)
{
	uint64_t stepsNow = 0;
	int increment = 1;

	while(1){
		int64_t delta = stepsTarget - stepsNow;

		//at position, nothing to do
		if (delta == 0){
			ESP_LOGW(TAG, "delta =0 waiting for change");
			vTaskDelay(300 / portTICK_PERIOD_MS);
			continue;
		}

		//define direction
		if (delta > 0){
			gpio_set_level(STEPPER_DIR_PIN, 0);
			increment = 1;
		} else {
			gpio_set_level(STEPPER_DIR_PIN, 1);
			increment = -1;
		}

		//do one step
		//note: triggers watchdog at long movements
		stepsNow += increment;
		gpio_set_level(STEPPER_STEP_PIN, 1);
		ets_delay_us(30);
		gpio_set_level(STEPPER_STEP_PIN, 0);
		ets_delay_us(90); //speed
	}
}

void stepperSw_setTargetSteps(uint64_t target){
	stepsTarget = target;
	char buffer[20];
	snprintf(buffer, sizeof(buffer), "%" PRIu64, target);
	ESP_LOGW(TAG, "set target steps to %s", buffer);
}









typedef struct {
	int targetSteps;        // Target step count
	int currentSteps;       // Current step count
	int acceleration;      // Acceleration (in steps per second^2)
	int deceleration;      // Deceleration (in steps per second^2)
	gpio_num_t pulsePin;           // Pin for pulse signal
	gpio_num_t directionPin;      // Pin for direction signal
	timer_group_t timerGroup;        // Timer group
	timer_idx_t timerIdx;          // Timer index
	bool isAccelerating;   // Flag to indicate if accelerating
	bool isDecelerating;   // Flag to indicate if decelerating
	int initialSpeed;      // Initial speed (in steps per second)
	int targetSpeed;       // Target speed (in steps per second)
	int currentSpeed;      // Current speed (in steps per second)
} StepperControl;

StepperControl ctrl;  // Create an instance of StepperControl struct



void stepper_setTargetSteps(int target_steps) {
	ESP_LOGW(TAG, "set target steps from %lld to %d  (stepsNow: %llu", (long long int)posTarget, target_steps, (long long int)posNow);
	posTarget = target_steps;
	//TODO switch dir pin in isr? not in sync with count
	//TODO switch direction using negative values as below

  // Update the targetSteps value
//ctrl.targetSteps = abs(target_steps);

  // Check if the timer is currently paused
	ESP_LOGW(TAG, "check if timer is running %d", timerIsRunning);
  if (!timerIsRunning){
    // If the timer is paused, start it again with the updated targetSteps
	timerIsRunning = true;
	ESP_LOGW(TAG, "starting timer because did not run before");
    ESP_ERROR_CHECK(timer_set_alarm_value(timerGroup, timerIdx, 1000));
    //timer_set_counter_value(timerGroup, timerIdx, 1000);
    ESP_ERROR_CHECK(timer_start(timerGroup, timerIdx));
	ESP_LOGW(TAG, "STARTED TIMER");
  }
}



void stepper_toggleDirection(){
	direction = !direction;
	gpio_set_level(ctrl.directionPin, direction);
	ESP_LOGW(TAG, "toggle direction -> %d", direction);
}







void stepper_init(){
	ESP_LOGW(TAG, "init - configure struct...");
	// Set values in StepperControl struct
	ctrl.targetSteps = 0;
	ctrl.currentSteps = 0;
	ctrl.acceleration = 100;
	ctrl.deceleration = 100;
	ctrl.pulsePin = STEPPER_STEP_PIN;
	ctrl.directionPin = STEPPER_DIR_PIN;
	ctrl.timerGroup = TIMER_GROUP_0;
	ctrl.timerIdx = TIMER_0;
	ctrl.isAccelerating = true;
	ctrl.isDecelerating = false;
	ctrl.initialSpeed = 0;  // Set initial speed as needed
	ctrl.targetSpeed = 500000;   // Set target speed as needed
	ctrl.currentSpeed = ctrl.initialSpeed;

	// Configure pulse and direction pins as outputs
	ESP_LOGW(TAG, "init - configure gpio pins...");
	gpio_set_direction(ctrl.pulsePin, GPIO_MODE_OUTPUT);
	gpio_set_direction(ctrl.directionPin, GPIO_MODE_OUTPUT);


	ESP_LOGW(TAG, "init - initialize/configure timer...");
	timer_config_t timer_conf = {
		.alarm_en = TIMER_ALARM_EN,         // we need alarm
		.counter_en = TIMER_PAUSE,          // dont start now lol
		.intr_type = TIMER_INTR_LEVEL,      // interrupt
		.counter_dir = TIMER_COUNT_UP,      // count up duh
		.auto_reload = TIMER_AUTORELOAD_EN, // reload pls
		.divider = 80000000ULL / TIMER_F,   // ns resolution
	};


	ESP_ERROR_CHECK(timer_init(ctrl.timerGroup, ctrl.timerIdx, &timer_conf));                   // init the timer
	ESP_ERROR_CHECK(timer_set_counter_value(ctrl.timerGroup, ctrl.timerIdx, 0));                // set it to 0
	//ESP_ERROR_CHECK(timer_isr_callback_add(ctrl.timerGroup, ctrl.timerIdx, timer_isr, )); // add callback fn to run when alarm is triggrd
	ESP_ERROR_CHECK(timer_isr_callback_add(ctrl.timerGroup, ctrl.timerIdx, timer_isr, (void *)ctrl.timerIdx, 0));
}




bool timer_isr(void *arg) {
	//turn pin on (fast)
	GPIO.out_w1ts = (1ULL << STEPPER_STEP_PIN);


	uint32_t speedTarget = 100000;
	uint32_t speedMin = 20000;
	//FIXME increment actually has to be re-calculated every run to have linear accel (because also gets called faster/slower)
	uint32_t decel_increment = 200;
	uint32_t accel_increment = 150;

	static uint64_t stepsToGo = 0;
	static uint32_t speedNow = speedMin;


	//--- define direction, stepsToGo ---
	//int64_t delta = (int)posTarget - (int)posNow;
	//bool directionTarget = delta >= 0 ? 1 : 0;
	bool directionTarget;
	if (posTarget > posNow) {
		directionTarget = 1;
	} else {
		directionTarget = 0;
	}

	//directionTarget = 1;
	//direction = 1;
	//gpio_set_level(STEPPER_DIR_PIN, direction);

	if (direction != directionTarget) {
		//ESP_LOGW(TAG, "direction differs! new: %d", direction);
		if (stepsToGo == 0){
			direction = directionTarget; //switch direction if almost idle
			gpio_set_level(STEPPER_DIR_PIN, direction);
			//stepsToGo = abs((int64_t)posTarget - (int64_t)posNow);
			stepsToGo = 2;
		} else {
			//stepsToGo = speedNow / decel_increment; //set steps to decel to min speed
			stepsToGo = speedNow / decel_increment;
		}
	} else if (direction == 1) {
		stepsToGo = posTarget - posNow;
		//stepsToGo = abs((int64_t)posTarget - (int64_t)posNow);
	} else {
		stepsToGo = posNow - posTarget;
		//stepsToGo = abs((int64_t)posTarget - (int64_t)posNow);
	}

	//TODO fix direction code above currently ony works with the below line instead
	//stepsToGo = abs((int64_t)posTarget - (int64_t)posNow);


	//--- define speed ---
	//uint64_t stepsAccelRemaining = (speedTarget - speedNow) / accel_increment;
	uint64_t stepsDecelRemaining = (speedNow - speedMin) / decel_increment;

	if (stepsToGo <= stepsDecelRemaining) {
		if ((speedNow - speedMin) > decel_increment) {
			speedNow -= decel_increment;
		} else {
			speedNow = speedMin; //PAUSE HERE??? / irrelevant?
		}
	}
	else if (speedNow < speedTarget) {
		speedNow += accel_increment;
		if (speedNow > speedTarget) speedNow = speedTarget;
	}
	else { //not relevant?
		speedNow = speedTarget;
	}


	//--- update timer ---
	if (stepsToGo == 0) {
		timer_pause(timerGroup, timerIdx);
		timerIsRunning = false;
	}
	else {
		ESP_ERROR_CHECK(timer_set_alarm_value(timerGroup, timerIdx, TIMER_BASE_CLK / speedNow));
	}


	//--- increment position ---
	if (stepsToGo > 0){
		stepsToGo --; //TODO increment at start, check at start??
	}
	if (direction == 1){
		posNow ++;
	} else {
		posNow --; //FIXME posNow gets extremely big after second start (underflow?)
	}


	// Generate pulse for stepper motor
	//gpio_set_level(STEPPER_STEP_PIN, 1);
	//ets_delay_us(500);  // Adjust delay as needed
	//gpio_set_level(STEPPER_STEP_PIN, 0);

	//turn pin off (fast)
	GPIO.out_w1tc = (1ULL << STEPPER_STEP_PIN);


//	// Cast arg to an integer type that has the same size as timer_idx_t
//	uintptr_t arg_val = (uintptr_t)arg;
//	// Cast arg_val to timer_idx_t
//	timer_idx_t timer_idx = (timer_idx_t)arg_val;
//	int32_t step_diff = ctrl.targetSteps - ctrl.currentSteps;
//	timerIsRunning = true;
//
//	if (timer_idx == ctrl.timerIdx) {
//		if (ctrl.currentSteps < ctrl.targetSteps) {
//			// Check if accelerating
//			if (ctrl.isAccelerating) {
//				if (ctrl.currentSpeed < ctrl.targetSpeed) {
//					// Increase speed if not yet at target speed
//					ctrl.currentSpeed += ctrl.acceleration;
//				} else {
//					// Reached target speed, clear accelerating flag
//					ctrl.isAccelerating = false;
//				}
//			}
//			//FIXME controller crashes when finished accelerating
//			//Guru Meditation Error: Core  0 panic'ed (Interrupt wdt timeout on CPU0).
//
//			// Check if decelerating
//			if (ctrl.isDecelerating) { //FIXME isDecelerating is never set???
//				if (ctrl.currentSpeed > ctrl.targetSpeed) {
//					// Decrease speed if not yet at target speed
//					ctrl.currentSpeed -= ctrl.deceleration;
//				} else {
//					// Reached target speed, clear decelerating flag
//					ctrl.isDecelerating = false;
//				}
//			}
//
//			// Generate pulse for stepper motor
//			gpio_set_level(ctrl.pulsePin, 1);
//			ets_delay_us(500);  // Adjust delay as needed
//			gpio_set_level(ctrl.pulsePin, 0);
//
//			// Update current step count
//			ctrl.currentSteps++;
//
//			// Update timer period based on current speed
//			timer_set_alarm_value(ctrl.timerGroup, ctrl.timerIdx, TIMER_BASE_CLK / ctrl.currentSpeed);
//		} else {
//			// Reached target step count, stop timer
//			timer_pause(ctrl.timerGroup, ctrl.timerIdx);
//			timerIsRunning = false;
//			//ESP_LOGW(TAG,"finished, pausing timer");
//		}
//	}
	return 1;
}


