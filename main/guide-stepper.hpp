extern "C"
{
#include <stdio.h>
#include "DendoStepper.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/adc.h"
}

#include "config.hpp"


//task that initializes and controls the stepper motor
//current functionality: 
//  - automatically auto-homes
//  - moves left and right repeatedly
//  - updates speed from potentiometer each cycle
void task_stepper(void *pvParameter);
