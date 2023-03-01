extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/adc.h"
}

#include "DendoStepper.h"
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
#define MIN 0
#define MAX 60 


#define SPEED_MIN 2.0   //mm/s
#define SPEED_MAX 60.0 //mm/s

#define ACCEL_MS 100.0  //ms from 0 to max
#define DECEL_MS 90.0   //ms from max to 0

#define STEPPER_STEPS_PER_ROT 1600
#define STEPPER_STEPS_PER_MM STEPPER_STEPS_PER_ROT/4

#define D_CABLE 6
#define D_REEL 155 //actual 170
#define PI 3.14159



//----------------------
//----- variables ------
//----------------------
static DendoStepper step;
static const char *TAG = "stepper"; //tag for logging

static int stepp_lengthNow = 0; //length measured in mm
static int lengthPrev = 0; //length last check

static bool stepp_direction = true;
static uint64_t posNow = 0;




//----------------------
//----- functions ------
//----------------------
//move left and right or reverse while winding
void travelDist(int length){
    //uint64_t posNow = step.getPositionMm();

        int d, remaining;
    d = abs(length);
    if(length < 0) stepp_direction = !stepp_direction; //invert direction in reverse mode

    while (d != 0){
        //--- currently moving right ---
        if (stepp_direction == true){         //currently moving right
            remaining = MAX - posNow;   //calc remaining distance fom current position to limit
            if (d > remaining){         //new distance will exceed limit
                step.runAbsMm (MAX);        //move to limit
                                          posNow=MAX;
                stepp_direction = false;      //change current direction for next iteration
                d = d - remaining;      //decrease target length by already traveled distance
                printf(" --- changed direction (L) --- \n ");
            }
            else {                      //target distance does not reach the limit
                step.runAbsMm (posNow + d); //move by (remaining) distance to reach target length
                printf("moving to %lld\n", posNow+d);
                posNow += d;
                d = 0;                  //finished, reset target length (could as well exit loop/break)
            }
        }

        //--- currently moving left ---
        else {
            remaining = posNow - MIN;
            if (d > remaining){
                step.runAbsMm (MIN);
                posNow=0;
                stepp_direction = true;
                d = d - remaining;
                printf(" --- changed direction (R) --- \n ");
            }
            else {
                step.runAbsMm (posNow - d); //when moving left the coordinate has to be decreased
                printf("moving to %lld\n", posNow-d);
                posNow -= d;
                d = 0;
            }
        }
    }

    if(length < 0) stepp_direction = !stepp_direction; //undo inversion of stepp_direction after reverse mode is finished
    return;



}






//calculate time needed for certain length (NOT NEEDED/ DELETE)
//float mmToS(int l){
//
//    double accel = SPEED_MAX / (ACCEL_MS/1000);
//    double t_accelMax = ACCEL_MS/1000;
//    double l_accelMax = accel * t_accelMax * t_accelMax;
//
//    printf("accel=%.2lf mm/s2 __ t_accelMAX=%.2lfs __ l_accelMax=%.2lfmm\n", accel, t_accelMax, l_accelMax);
//    if (l < l_accelMax){
//        return sqrt(l / accel);
//    } else {
//        return t_accelMax +  (l - l_accelMax) / SPEED_MAX;
//    }
//}


//define zero/start position 
//currently crashes into hardware limitation for certain time 
//TODO: limit switch
void home() {
    ESP_LOGW(TAG, "auto-home...");
    step.setSpeedMm(120, 120, 100);
    step.runInf(0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    step.stop();
    step.resetAbsolute();
    ESP_LOGW(TAG, "auto-home finished");
}


//initialize/configure stepper instance
void init_stepper() {
    ESP_LOGW(TAG, "initializing stepper...");
    DendoStepper_config_t step_cfg = {
        .stepPin = STEPPER_STEP_PIN,
        .dirPin = STEPPER_DIR_PIN,
        .enPin = STEPPER_EN_PIN,
        .timer_group = TIMER_GROUP_0,
        .timer_idx = TIMER_0,
        .miStep = MICROSTEP_32,
        .stepAngle = 1.8};
    step.config(&step_cfg);
    step.init();

    step.setSpeed(1000, 1000, 1000); //random default speed
    step.setStepsPerMm(STEPPER_STEPS_PER_MM); //guide: 4mm/rot
}


//function that updates speed value using ADC input and configured MIN/MAX
void updateSpeedFromAdc() {
    int potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095 GPIO34
    double poti = potiRead/4095.0;
    int speed = poti*(SPEED_MAX-SPEED_MIN) + SPEED_MIN;
    step.setSpeedMm(speed, ACCEL_MS, DECEL_MS);
    ESP_LOGW(TAG, "poti: %d (%.2lf%%), set speed to: %d", potiRead, poti*100, speed);
}



//---------------------
//------- TASK --------
//---------------------
void task_stepper_test(void *pvParameter)
{
    init_stepper();
    home();

    while (1) {
        updateSpeedFromAdc();
        step.runPosMm(-STEPPER_TEST_TRAVEL);
        while(step.getState() != 1) vTaskDelay(2);
        ESP_LOGI(TAG, "finished moving right => moving left");

        updateSpeedFromAdc();
        step.runPosMm(STEPPER_TEST_TRAVEL);
        while(step.getState() != 1) vTaskDelay(2); //1=idle
        ESP_LOGI(TAG, "finished moving left => moving right");
    }
}


void task_stepper_ctl(void *pvParameter)
{
    init_stepper();
    home();

    while(1){
        //get current length
        stepp_lengthNow = encoder_getLenMm();

        int lengthDelta = stepp_lengthNow - lengthPrev;
        //TODO add modifier e.g. poti value
        int travel = lengthDelta * D_CABLE / (PI * D_REEL); //calc distance to move
        ESP_LOGI(TAG, "delta: %d travel: %d",lengthDelta, travel);
                                                            //FIXME: rounding issue? e.g. 1.4 gets lost
        if (abs(travel) > 1){ //when ready to move axis by at least 1 millimeter
        ESP_LOGI(TAG, "EXCEEDED: delta: %d travel: %d",lengthDelta, travel);
            travelDist(travel);
            while(step.getState() != 1) vTaskDelay(2); //wait for move to finish
            lengthPrev = stepp_lengthNow; //update length
        }
        else {
            vTaskDelay(5);
        }
    }

}
