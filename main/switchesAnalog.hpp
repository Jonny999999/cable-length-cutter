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


#define GPIO_BUTTONS GPIO_NUM_34
#define ADC_CHANNEL_BUTTONS ADC1_CHANNEL_6 //gpio 34



void switchesAnalog_handle();
