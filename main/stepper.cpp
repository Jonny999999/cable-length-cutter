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


//=====================
//=== configuration ===
//=====================
//used macros from config.hpp:
//#define STEPPER_STEP_PIN GPIO_NUM_18    //mos1
//#define STEPPER_DIR_PIN GPIO_NUM_16     //ST3

//#define STEPPER_STEPS_PER_MM	200/2	//steps/mm (steps-per-rot / slope)
//#define STEPPER_SPEED_DEFAULT	20		//mm/s
//#define STEPPER_SPEED_MIN		4		//mm/s  - speed threshold at which stepper immediately starts/stops
//#define STEPPER_ACCEL_INC		3		//steps/s increment per cycle 
//#define STEPPER_DECEL_INC		8		//steps/s decrement per cycle

#define TIMER_F 1000000ULL
#define TICK_PER_S TIMER_S
#define NS_TO_T_TICKS(x) (x)



//========================
//=== global variables ===
//========================
static const char *TAG = "stepper-driver"; //tag for logging
									
bool direction = 1;
bool directionTarget = 1;
bool timerIsRunning = false;
bool timer_isr(void *arg);

static timer_group_t timerGroup = TIMER_GROUP_0;
static timer_idx_t timerIdx = TIMER_0;

//TODO the below variables can be moved to isr function once debug output is no longer needed
static uint64_t posTarget = 0;
static uint64_t posNow = 0;
static uint64_t stepsToGo = 0;
static uint32_t speedMin = STEPPER_SPEED_MIN * STEPPER_STEPS_PER_MM;
static uint32_t speedNow = speedMin;
static int debug = 0;
static uint32_t speedTarget = STEPPER_SPEED_DEFAULT * STEPPER_STEPS_PER_MM;
//TODO/NOTE increment actually has to be re-calculated every run to have linear accel (because also gets called faster/slower)
static uint32_t decel_increment = STEPPER_DECEL_INC;
static uint32_t accel_increment = STEPPER_ACCEL_INC;



//======================
//===== DEBUG task =====
//======================
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
				"speedTarget=%u "
				"debug=%d ",

				timerIsRunning,
				direction, 
				directionTarget, 
				posTarget, 
				posNow, 
				stepsToGo,
				speedNow,
				speedTarget,
				debug
				);

			vTaskDelay(300 / portTICK_PERIOD_MS);
	}
}



//=====================
//===== set speed =====
//=====================
void stepper_setSpeed(uint32_t speedMmPerS) {
	ESP_LOGW(TAG, "set target speed from %u to %u mm/s  (%u steps/s)",
			speedTarget, speedMmPerS, speedMmPerS * STEPPER_STEPS_PER_MM);
	speedTarget = speedMmPerS * STEPPER_STEPS_PER_MM;
}



//==========================
//== set target pos STEPS ==
//==========================
void stepper_setTargetPosSteps(uint64_t target_steps) {
	ESP_LOGW(TAG, "update target position from %llu to %llu  steps (stepsNow: %llu", posTarget, target_steps, posNow);
	posTarget = target_steps;

  // Check if the timer is currently paused
  if (!timerIsRunning){
    // If the timer is paused, start it again with the updated targetSteps
	timerIsRunning = true;
	ESP_LOGW(TAG, "starting timer because did not run before");
    ESP_ERROR_CHECK(timer_set_alarm_value(timerGroup, timerIdx, 1000));
    //timer_set_counter_value(timerGroup, timerIdx, 1000);
    ESP_ERROR_CHECK(timer_start(timerGroup, timerIdx));
  }
}



//=========================
//=== set target pos MM ===
//=========================
void stepper_setTargetPosMm(uint32_t posMm){
	ESP_LOGW(TAG, "set target position to %u mm", posMm);
	stepper_setTargetPosSteps(posMm * STEPPER_STEPS_PER_MM);
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

	//-----------------
	//--- variables ---
	//-----------------
	//TODO used (currently global) variables here

	//-----------------------------------
	//--- define direction, stepsToGo ---
	//-----------------------------------
	//Note: the idea is that the stepper has to decelerate to min speed first before changeing the direction
	//define target direction depending on position difference
	bool directionTarget = posTarget > posNow ? 1 : 0;
	//DIRECTION DIFFERS (change)
	if ( (direction != directionTarget) && (posTarget != posNow)) {
		if (stepsToGo == 0){ //standstill
			direction = directionTarget; //switch direction
			gpio_set_level(STEPPER_DIR_PIN, direction);
			stepsToGo = abs(int64_t(posTarget - posNow));
		} else {
			//set to minimun decel steps
			stepsToGo = (speedNow - speedMin) / decel_increment;
		}
	}
	//NORMAL (any direction 0/1)
	else {
		stepsToGo = abs(int64_t(posTarget - posNow));
	}

	//--------------------
	//--- define speed ---
	//--------------------
	//FIXME noticed crash: division by 0 when min speed > target speed
	uint64_t stepsDecelRemaining = (speedNow - speedMin) / decel_increment;
	//DECELERATE
	if (stepsToGo <= stepsDecelRemaining) {
		//FIXME if stepsToGo gets updated (lowered) close to target while close to target, the stepper may stop too fast -> implement possibility to 'overshoot and reverse'?
		if ((speedNow - speedMin) > decel_increment) {
			speedNow -= decel_increment;
		} else {
			speedNow = speedMin; //PAUSE HERE??? / irrelevant?
		}
	}
	//ACCELERATE
	else if (speedNow < speedTarget) {
		speedNow += accel_increment;
		if (speedNow > speedTarget) speedNow = speedTarget;
	}
	//COASTING
	else { //not relevant?
		speedNow = speedTarget;
	}

	//-------------------------------
	//--- update timer, increment ---
	//-------------------------------
	//AT TARGET -> STOP
	if (stepsToGo == 0) {
		timer_pause(timerGroup, timerIdx);
		timerIsRunning = false;
		speedNow = speedMin;
		return 1;
	}

	//STEPS REMAINING -> NEXT STEP
	//update timer with new speed
	ESP_ERROR_CHECK(timer_set_alarm_value(timerGroup, timerIdx, TIMER_F / speedNow));

	//generate pulse
	GPIO.out_w1ts = (1ULL << STEPPER_STEP_PIN); //turn on (fast)
	ets_delay_us(10);
	GPIO.out_w1tc = (1ULL << STEPPER_STEP_PIN); //turn off (fast)

	//increment pos
	stepsToGo --;
	if (direction == 1){
		posNow ++;
	} else {
		//prevent underflow FIXME this case should not happen in the first place?
		if (posNow != 0){
			posNow --;
		} else {
			ESP_LOGE(TAG,"isr: posNow would be negative - ignoring decrement");
		}
	}
	return 1;
}


