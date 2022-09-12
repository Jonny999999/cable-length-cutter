#pragma once
extern "C"
{
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include "esp_log.h"
#include "driver/adc.h"
#include <math.h>
}

#include "config.hpp"
#include "gpio_adc.hpp"


//get current state of certain switch
bool switchesAnalog_getState(int swNumber);

bool switchesAnalog_getState_sw0();
bool switchesAnalog_getState_sw1();
bool switchesAnalog_getState_sw2();
bool switchesAnalog_getState_sw3();
