#pragma once
#include <stdio.h>
#include "driver/adc.h"

//function for multisampling an anlog input
//measures 30 times and returns average
//if invertion is used currently 11bit resolution is assumed (subtracts from 4095) 
//TODO: rework this function to be more universal
int gpio_readAdc(adc1_channel_t adc_channel, bool inverted = false);
