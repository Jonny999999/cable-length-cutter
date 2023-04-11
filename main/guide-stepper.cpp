extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/adc.h"
}

#include "stepper.hpp"
#include "config.hpp"
#include "guide-stepper.hpp"
#include "encoder.hpp"



//---------------------
//--- configuration ---
//---------------------
//also see config.hpp
//for pin definition

#define STEPPER_TEST_TRAVEL 65     //mm
                                   //
#define MIN_MM 0
#define MAX_MM 60 
#define POS_MAX_STEPS MAX_MM * STEPPER_STEPS_PER_MM
#define POS_MIN_STEPS MIN_MM * STEPPER_STEPS_PER_MM


#define SPEED_MIN 2.0   //mm/s
#define SPEED_MAX 60.0 //mm/s

#define ACCEL_MS 100.0  //ms from 0 to max
#define DECEL_MS 90.0   //ms from max to 0

#define STEPPER_STEPS_PER_ROT 1600
#define STEPPER_STEPS_PER_MM STEPPER_STEPS_PER_ROT/4

#define D_CABLE 6
#define D_REEL 160 //actual 170
#define PI 3.14159



//----------------------
//----- variables ------
//----------------------
static const char *TAG = "stepper"; //tag for logging

static bool stepp_direction = true;
static uint32_t posNow = 0;



//----------------------
//----- functions ------
//----------------------
////move axis certain Steps (relative) between left and right or reverse when negative
//void travelSteps(int stepsTarget){
//    //posNow = step.getPositionMm(); //not otherwise controlled, so no update necessary
//    int stepsToGo, remaining;
//
//    stepsToGo = abs(stepsTarget);
//    if(stepsTarget < 0) stepp_direction = !stepp_direction; //invert direction in reverse mode
//
//    while (stepsToGo != 0){
//        //--- currently moving right ---
//        if (stepp_direction == true){               //currently moving right
//            remaining = POS_MAX_STEPS - posNow;     //calc remaining distance fom current position to limit
//            if (stepsToGo > remaining){             //new distance will exceed limit
//                //....step.runAbs (POS_MAX_STEPS);        //move to limit
//                //....while(step.getState() != 1) vTaskDelay(1);  //wait for move to finish
//                posNow = POS_MAX_STEPS;
//                stepp_direction = false;            //change current direction for next iteration
//                stepsToGo = stepsToGo - remaining;  //decrease target length by already traveled distance
//                ESP_LOGI(TAG, " --- moved to max -> change direction (L) --- \n ");
//            }
//            else {                                  //target distance does not reach the limit
//                //....step.runAbs (posNow + stepsToGo);   //move by (remaining) distance to reach target length
//                //....while(step.getState() != 1) vTaskDelay(1); //wait for move to finish
//                ESP_LOGD(TAG, "moving to %d\n", posNow+stepsToGo);
//                posNow += stepsToGo;
//                stepsToGo = 0;                      //finished, reset target length (could as well exit loop/break)
//            }
//        }
//
//        //--- currently moving left ---
//        else {
//            remaining = posNow - POS_MIN_STEPS;
//            if (stepsToGo > remaining){
//                //....step.runAbs (POS_MIN_STEPS);
//                //....while(step.getState() != 1) vTaskDelay(2);  //wait for move to finish
//                posNow = POS_MIN_STEPS;
//                stepp_direction = true;
//                stepsToGo = stepsToGo - remaining;
//                ESP_LOGI(TAG, " --- moved to min -> change direction (R) --- \n ");
//            }
//            else {
//                //....step.runAbs (posNow - stepsToGo);           //when moving left the coordinate has to be decreased
//                while(step.getState() != 1) vTaskDelay(2);  //wait for move to finish
//                ESP_LOGD(TAG, "moving to %d\n", posNow - stepsToGo);
//                posNow -= stepsToGo;
//                stepsToGo = 0;
//            }
//        }
//    }
//    if(stepsTarget < 0) stepp_direction = !stepp_direction; //undo inversion of stepp_direction after reverse mode is finished
//    return;
//}
//
//
////move axis certain Mm (relative) between left and right or reverse when negative
//void travelMm(int length){
//    travelSteps(length * STEPPER_STEPS_PER_MM);
//}
//
//
////define zero/start position 
////currently crashes into hardware limitation for certain time 
////TODO: limit switch
//void home() {
//    ESP_LOGW(TAG, "auto-home...");
//    //....step.setSpeedMm(100, 500, 10);
//    //....step.runInf(1);
//    vTaskDelay(1500 / portTICK_PERIOD_MS);
//    //....step.stop();
//    //....step.resetAbsolute();
//    ESP_LOGW(TAG, "auto-home finished");
//}


//initialize/configure stepper instance
void init_stepper() {
//    ESP_LOGW(TAG, "initializing stepper...");
//    DendoStepper_config_t step_cfg = {
//        .stepPin = STEPPER_STEP_PIN,
//        .dirPin = STEPPER_DIR_PIN,
//        .enPin = STEPPER_EN_PIN,
//        .timer_group = TIMER_GROUP_0,
//        .timer_idx = TIMER_0,
//        .miStep = MICROSTEP_32,
//        .stepAngle = 1.8};
//    //....step.config(&step_cfg);
//    //....step.init();
//
//    //....step.setSpeed(1000, 1000, 1000); //random default speed
//    //....step.setStepsPerMm(STEPPER_STEPS_PER_MM); //guide: 4mm/rot

	stepper_init();
}


//function that updates speed value using ADC input and configured MIN/MAX
void updateSpeedFromAdc() {
    int potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095 GPIO34
    double poti = potiRead/4095.0;
    int speed = poti*(SPEED_MAX-SPEED_MIN) + SPEED_MIN;
    //....step.setSpeedMm(speed, ACCEL_MS, DECEL_MS);
    ESP_LOGW(TAG, "poti: %d (%.2lf%%), set speed to: %d", potiRead, poti*100, speed);
}



//----------------------------
//---- TASK stepper-test -----
//----------------------------
void task_stepper_test(void *pvParameter)
{
	stepper_init();
	while(1){
		vTaskDelay(20 / portTICK_PERIOD_MS);

		//------ handle switches ------
		//run handle functions for all switches
		SW_START.handle();
		SW_RESET.handle();
		SW_SET.handle();
		SW_PRESET1.handle();
		SW_PRESET2.handle();
		SW_PRESET3.handle();
		SW_CUT.handle();
		SW_AUTO_CUT.handle();

		if (SW_RESET.risingEdge) {
			stepper_toggleDirection();
			buzzer.beep(1, 1000, 100);
		}
		if (SW_PRESET1.risingEdge) {
			buzzer.beep(2, 300, 100);
			stepper_setTargetSteps(1000);
		}
		if (SW_PRESET2.risingEdge) {
			buzzer.beep(1, 500, 100);
			stepper_setTargetSteps(10000);
		}
		if (SW_PRESET3.risingEdge) {
			buzzer.beep(1, 100, 100);
			stepper_setTargetSteps(60000);
		}
	}
}




//----------------------------
//----- TASK stepper-ctl -----
//----------------------------
void task_stepper_ctl(void *pvParameter)
{
//    //variables
//    int encStepsNow = 0; //get curret steps of encoder
//    int encStepsPrev = 0; //steps at last check
//    int encStepsDelta = 0; //steps changed since last iteration
//
//    double cableLen = 0;
//    double travelStepsExact = 0; //steps axis has to travel
//    double travelStepsPartial = 0;
//    int travelStepsFull = 0;
//    double travelMm = 0;
//    double turns = 0;
//
//    float potiModifier;
//
//    init_stepper();
//    home();
//
//    while(1){
//        //get current length
//        encStepsNow = encoder_getSteps();
//
//        //calculate change
//        encStepsDelta = encStepsNow - encStepsPrev; //FIXME MAJOR BUG: when resetting encoder/length in control task, diff will be huge!
//
//        //read potentiometer and normalize (0-1) to get a variable for testing
//        potiModifier = (float) gpio_readAdc(ADC_CHANNEL_POTI) / 4095; //0-4095 -> 0-1
//        //ESP_LOGI(TAG, "current poti-modifier = %f", potiModifier);
//
//        //calculate steps to move
//        cableLen = (double)encStepsDelta * 1000 / ENCODER_STEPS_PER_METER;
//        turns = cableLen / (PI * D_REEL);
//        travelMm = turns * D_CABLE;
//        travelStepsExact = travelMm * STEPPER_STEPS_PER_MM  +  travelStepsPartial; //convert mm to steps and add not moved partial steps
//        travelStepsPartial = 0;
//        travelStepsFull = (int)travelStepsExact;
//
//        //move axis when ready to move at least 1 step
//        if (abs(travelStepsFull) > 1){
//            travelStepsPartial = fmod(travelStepsExact, 1); //save remaining partial steps to be added in the next iteration
//            ESP_LOGD(TAG, "cablelen=%.2lf, turns=%.2lf, travelMm=%.3lf, travelStepsExact: %.3lf, travelStepsFull=%d, partialStep=%.3lf", cableLen, turns, travelMm, travelStepsExact, travelStepsFull, travelStepsPartial);
//            ESP_LOGI(TAG, "MOVING %d steps", travelStepsFull);
//            //TODO: calculate variable speed for smoother movement? for example intentionally lag behind and calculate speed according to buffered data
//            //....step.setSpeedMm(35, 100, 50);
//            //testing: get speed from poti
//            //step.setSpeedMm(35, 1000*potiModifier+1, 1000*potiModifier+1);
//            travelSteps(travelStepsExact);
//            encStepsPrev = encStepsNow; //update previous length
//        }
//        else {
//            //TODO use encoder queue to only run this check at encoder event?
//            vTaskDelay(2);
//        }
//    }
}
