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

#include "max7219.h"

}
#include <cmath>

#include "config.hpp"
#include "gpio_evaluateSwitch.hpp"
#include "gpio_adc.hpp"
#include "buzzer.hpp"
#include "vfd.hpp"
#include "display.hpp"
#include "cutter.hpp"



//enum describing the state of the system
enum class systemState_t {COUNTING, WINDING_START, WINDING, TARGET_REACHED, AUTO_CUT_WAITING, CUTTING, MANUAL};
//array with enum as strings for logging states
extern const char* systemStateStr[7];

//task that controls the entire machine (has to be created as task in main function)
void task_control(void *pvParameter);
