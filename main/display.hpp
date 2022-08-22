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

//function for initializing the display using configuration from macros in config.hpp
max7219_t display_init();

//show welcome message on the entire display
void display_ShowWelcomeMsg(max7219_t displayDevice);


class handledDisplay {
    public:
        //--- constructor ---
        //TODO add posMax to prevent writing in segments of other instance
        handledDisplay(max7219_t displayDevice, uint8_t posStart);

        //--- methods ---
        void showString(const char * buf, uint8_t pos = 0);
        //function switches between two strings in a given interval
        void blinkStrings(const char * strOn, const char * strOff, uint32_t msOn, uint32_t msOff);
        //TODO: add 'scroll string' method
        //function that handles blinking of display
        void handle();

    private:

        //--- variables ---
        //config
        max7219_t dev;
        uint8_t posStart; //absolute position this display instance starts (e.g. multiple or very long 7 segment display)

        //blink mode
        char strOn[20];
        char strOff[20];
        bool state = false;
        bool blinkMode = false;
        uint32_t msOn;
        uint32_t msOff;
        uint32_t timestampOn;
        uint32_t timestampOff;
};
