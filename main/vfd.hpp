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

//enum defining motor direction
typedef enum vfd_direction_t {FWD, REV} vfd_direction_t;
//strubg array to be able to log direction state as string
extern const char* vfd_directionStr[2];

//function for setting the state and optional direction of the motor: on/off, FWD/REV (default FWD)
void vfd_setState(bool stateNew, vfd_direction_t direction = FWD);

//function for setting the speed level (0-3)
void vfd_setSpeedLevel(uint8_t levelNew = 0);
