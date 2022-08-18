#pragma once
extern "C"
{
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_idf_version.h>
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/adc.h"

#include <max7219.h>
#include "rotary_encoder.h"
}
#include <cmath>

#include "config.hpp"
#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "vfd.hpp"



typedef enum systemState_t {COUNTING, WINDING_START, WINDING, TARGET_REACHED} systemState_t;
extern const char* systemStateStr[4];


void task_control(void *pvParameter);
