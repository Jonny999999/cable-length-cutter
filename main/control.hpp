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

#include <max7219.h>
#include "rotary_encoder.h"
}

#include "config.hpp"
#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"



void task_control(void *pvParameter);
