//custom driver for stepper motor

#include "config.hpp"


//config from config.hpp
//#define STEPPER_STEP_PIN GPIO_NUM_18    //mos1
//#define STEPPER_DIR_PIN GPIO_NUM_16     //ST3
//

extern "C" {
#include "driver/timer.h"
#include "driver/gpio.h"
}

#define TIMER_F 1000000ULL
#define TICK_PER_S TIMER_S
#define NS_TO_T_TICKS(x) (x)

static const char *TAG = "stepper-ctl"; //tag for logging
bool direction = 0;
bool timerIsRunning = false;

bool timer_isr(void *arg);


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
	ESP_LOGW(TAG, "set target steps to %d", target_steps);
	//TODO switch dir pin in isr? not in sync with count
	//TODO switch direction using negative values as below
//	if(target_steps < 0){
//		gpio_set_level(ctrl.directionPin, 1);
//	} else {
//		gpio_set_level(ctrl.directionPin, 0);
//	}
	ESP_LOGW(TAG, "toggle direction -> %d", direction);
	

  // Update the targetSteps value
  ctrl.targetSteps = abs(target_steps);

  // Check if the timer is currently paused
  if (!timerIsRunning){
    // If the timer is paused, start it again with the updated targetSteps
    timer_set_alarm_value(ctrl.timerGroup, ctrl.timerIdx, 1000);
    timer_set_counter_value(ctrl.timerGroup, ctrl.timerIdx, 1000);
    timer_start(ctrl.timerGroup, ctrl.timerIdx);
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
	// Cast arg to an integer type that has the same size as timer_idx_t
	uintptr_t arg_val = (uintptr_t)arg;
	// Cast arg_val to timer_idx_t
	timer_idx_t timer_idx = (timer_idx_t)arg_val;
	int32_t step_diff = ctrl.targetSteps - ctrl.currentSteps;
	timerIsRunning = true;

	if (timer_idx == ctrl.timerIdx) {
		if (ctrl.currentSteps < ctrl.targetSteps) {
			// Check if accelerating
			if (ctrl.isAccelerating) {
				if (ctrl.currentSpeed < ctrl.targetSpeed) {
					// Increase speed if not yet at target speed
					ctrl.currentSpeed += ctrl.acceleration;
				} else {
					// Reached target speed, clear accelerating flag
					ctrl.isAccelerating = false;
				}
			}
			//FIXME controller crashes when finished accelerating
			//Guru Meditation Error: Core  0 panic'ed (Interrupt wdt timeout on CPU0).

			// Check if decelerating
			if (ctrl.isDecelerating) { //FIXME isDecelerating is never set???
				if (ctrl.currentSpeed > ctrl.targetSpeed) {
					// Decrease speed if not yet at target speed
					ctrl.currentSpeed -= ctrl.deceleration;
				} else {
					// Reached target speed, clear decelerating flag
					ctrl.isDecelerating = false;
				}
			}

			// Generate pulse for stepper motor
			gpio_set_level(ctrl.pulsePin, 1);
			ets_delay_us(500);  // Adjust delay as needed
			gpio_set_level(ctrl.pulsePin, 0);

			// Update current step count
			ctrl.currentSteps++;

			// Update timer period based on current speed
			timer_set_alarm_value(ctrl.timerGroup, ctrl.timerIdx, TIMER_BASE_CLK / ctrl.currentSpeed);
		} else {
			// Reached target step count, stop timer
			timer_pause(ctrl.timerGroup, ctrl.timerIdx);
			timerIsRunning = false;
			//ESP_LOGW(TAG,"finished, pausing timer");
		}
	}
	return 1;
}


