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
#include <cstring>


#include "config.hpp"

void display_init();
void display_ShowWelcomeMsg();
void display1_showString(const char * buf);
void display2_showString(const char * buf);
void display_showString(uint8_t pos, const char * buf);

//function that handles blinking of display2
void display2_handle();
//function switches between two strings in a given interval
void display2_blinkStrings(const char * strOn, const char * strOff, uint32_t msOn, uint32_t msOff);
