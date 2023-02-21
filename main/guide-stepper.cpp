#include "guide-stepper.hpp"

#define LEN_MOVE 90     //mm
#define SPEED_MIN 5.0   //mm/s
#define SPEED_MAX 110.0 //mm/s

#define ACCEL_MS 100.0  //ms from 0 to max
#define DECEL_MS 90.0   //ms from max to 0

#define STEPS_PER_ROT 1600



//----------------------
//----- variables ------
//----------------------
DendoStepper step;
DendoStepper step1;
static const char *TAG = "stepper"; //tag for logging



//----------------------
//----- functions ------
//----------------------
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
        .stepPin = 18,
        .dirPin = 19,
        .enPin = 21,
        .timer_group = TIMER_GROUP_0,
        .timer_idx = TIMER_0,
        .miStep = MICROSTEP_32,
        .stepAngle = 1.8};
    step.config(&step_cfg);
    step.init();

    step.setSpeed(1000, 1000, 1000); //random default speed
    step.setStepsPerMm(STEPS_PER_ROT /4); //guide: 4mm/rot
}


//function that updates speed value using ADC input and configured MIN/MAX
void updateSpeedFromAdc() {
    int potiRead = gpio_readAdc(ADC1_CHANNEL_6); //0-4095 GPIO34
    double poti = potiRead/4095.0;
    int speed = poti*(SPEED_MAX-SPEED_MIN) + SPEED_MIN;
    step.setSpeedMm(speed, ACCEL_MS, DECEL_MS);
    ESP_LOGW(TAG, "poti: %d (%.2lf%%), set speed to: %d", potiRead, poti*100, speed);
}



//---------------------
//------- TASK --------
//---------------------
void task_stepper(void *pvParameter)
{
    init_stepper();
    home();

    while (1) {
        updateSpeedFromAdc();
        step.runPosMm(-LEN_MOVE);
        while(step.getState() != 1) vTaskDelay(2);
        ESP_LOGI(TAG, "finished moving right => moving left");

        updateSpeedFromAdc();
        step.runPosMm(LEN_MOVE);
        while(step.getState() != 1) vTaskDelay(2); //1=idle
        ESP_LOGI(TAG, "finished moving left => moving right");
    }
}
