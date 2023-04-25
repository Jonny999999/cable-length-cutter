//custom driver for stepper motor
#include "config.hpp"
#include "hal/timer_types.h"
#include <cstdint>
#include <inttypes.h>

extern "C" {
#include "driver/timer.h"
#include "driver/gpio.h"
#include "esp_log.h"
}

//config from config.hpp
//#define STEPPER_STEP_PIN GPIO_NUM_18    //mos1
//#define STEPPER_DIR_PIN GPIO_NUM_16     //ST3

#define TIMER_F 1000000ULL
#define TICK_PER_S TIMER_S
#define NS_TO_T_TICKS(x) (x)



//========================
//=== global variables ===
//========================
static const char *TAG = "stepper-ctl"; //tag for logging
									
bool direction = 1;
bool directionTarget = 1;
bool timerIsRunning = false;
bool timer_isr(void *arg);

static timer_group_t timerGroup = TIMER_GROUP_0;
static timer_idx_t timerIdx = TIMER_0;

//move to isr
static uint64_t posTarget = 0;
static uint64_t posNow = 0;
static uint64_t stepsToGo = 0;
static uint32_t speedMin = 20000;
static uint32_t speedNow = speedMin;
static int debug = 0;



//===============================
//=== software - control task ===
//===============================
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

//=================================
//=== software - set target pos ===
//=================================
void stepperSw_setTargetSteps(uint64_t target){
	stepsTarget = target;
	char buffer[20];
	snprintf(buffer, sizeof(buffer), "%" PRIu64, target);
	ESP_LOGW(TAG, "set target steps to %s", buffer);
}




void task_stepper_debug(void *pvParameter){
	while (1){
		ESP_LOGI("stepper-DEBUG",
				"timer=%d "
				"dir=%d "
				"dirTarget=%d "
				"posTarget=%llu "
				"posNow=%llu "
				"stepsToGo=%llu "
				"speedNow=%u "
				"debug=%d ",

				timerIsRunning,
				direction, 
				directionTarget, 
				posTarget, 
				posNow, 
				stepsToGo,
				speedNow,
				debug
				);

			vTaskDelay(300 / portTICK_PERIOD_MS);
	}
}




//========================
//==== set target pos ====
//========================
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





//========================
//===== init stepper =====
//========================
void stepper_init(){
	ESP_LOGW(TAG, "init - configure struct...");

	// Configure pulse and direction pins as outputs
	ESP_LOGW(TAG, "init - configure gpio pins...");
	gpio_set_direction(STEPPER_DIR_PIN, GPIO_MODE_OUTPUT);
	gpio_set_direction(STEPPER_STEP_PIN, GPIO_MODE_OUTPUT);

	ESP_LOGW(TAG, "init - initialize/configure timer...");
	timer_config_t timer_conf = {
		.alarm_en = TIMER_ALARM_EN,         // we need alarm
		.counter_en = TIMER_PAUSE,          // dont start now lol
		.intr_type = TIMER_INTR_LEVEL,      // interrupt
		.counter_dir = TIMER_COUNT_UP,      // count up duh
		.auto_reload = TIMER_AUTORELOAD_EN, // reload pls
		.divider = 80000000ULL / TIMER_F,   // ns resolution
	};
	ESP_ERROR_CHECK(timer_init(timerGroup, timerIdx, &timer_conf));                   // init the timer
	ESP_ERROR_CHECK(timer_set_counter_value(timerGroup, timerIdx, 0));                // set it to 0
	ESP_ERROR_CHECK(timer_isr_callback_add(timerGroup, timerIdx, timer_isr, (void *)timerIdx, 0));
}



//================================
//=== timer interrupt function ===
//================================
bool timer_isr(void *arg) {
	// Generate pulse for stepper motor
	//turn pin on (fast)
	GPIO.out_w1ts = (1ULL << STEPPER_STEP_PIN);


	//--- variables ---
	static uint32_t speedTarget = 100000;
	//FIXME increment actually has to be re-calculated every run to have linear accel (because also gets called faster/slower)
	static uint32_t decel_increment = 200;
	static uint32_t accel_increment = 150;



	//--- define direction, stepsToGo ---
	//int64_t delta = (int)posTarget - (int)posNow;
	//bool directionTarget = delta >= 0 ? 1 : 0;
	if (posTarget >= posNow) {
		directionTarget = 1;
	} else {
		directionTarget = 0;
	}
	//directionTarget = 1;
	//direction = 1;
	//gpio_set_level(STEPPER_DIR_PIN, direction);
	if ( (direction != directionTarget) && (posTarget != posNow)) {
		//ESP_LOGW(TAG, "direction differs! new: %d", direction);
		if (stepsToGo == 0){
			direction = directionTarget; //switch direction if almost idle
			gpio_set_level(STEPPER_DIR_PIN, direction);
			//stepsToGo = abs((int64_t)posTarget - (int64_t)posNow);
			stepsToGo = 2;
		} else {
			//stepsToGo = speedNow / decel_increment; //set steps to decel to min speed
			//set to minimun decel steps
			stepsToGo = (speedNow - speedMin) / decel_increment;
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
		//prevent underflow FIXME this case should not happen in the first place?
		if (posNow != 0){
			posNow --;
		} else {
			//ERR posNow would be negative?
		}
	}


	// Generate pulse for stepper motor
	//turn pin off (fast)
	GPIO.out_w1tc = (1ULL << STEPPER_STEP_PIN);

	return 1;
}


