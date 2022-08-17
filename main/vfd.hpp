#pragma once
extern "C"
{
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
}

#include "config.hpp"


//function for setting the state (motor on/off)
void vfd_setState(bool state);

//function for setting the speed level (1-7)
void vfd_setSpeedLevel(uint8_t level = 0);
