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



//function for initializing the display using configuration from macros in config.h
max7219_t display_init();

//show welcome message on the entire display
void display_ShowWelcomeMsg(max7219_t displayDevice);

enum class displayMode {NORMAL, BLINK_STRINGS, BLINK};

class handledDisplay {
    public:
        //--- constructor ---
        //TODO add posMax to prevent writing in segments of other instance
        handledDisplay(max7219_t displayDevice, uint8_t posStart);

        //--- methods ---
        void showString(const char * buf, uint8_t pos = 0);
        //function switches between two strings in a given interval
        void blinkStrings(const char * strOn, const char * strOff, uint32_t msOn, uint32_t msOff);
        //triggers certain count of blinking between currently shown string and off or optional certain string
        void blink(uint8_t count, uint32_t msOn, uint32_t msOff, const char * strOff = "        ");
        //function that handles time based modes and writes text to display
        void handle(); //has to be run regularly when blink method is used

        //TODO: blinkStrings and blink are very similar - optimize?
        //TODO: add 'scroll string' method


    private:
        //--- variables ---
        //config
        max7219_t dev;
        uint8_t posStart; //absolute position this display instance starts (e.g. multiple or very long 7 segment display)
        uint8_t posCurrent;

        displayMode mode = displayMode::NORMAL;
        //blink modes
        uint8_t count = 0;
        char strOn[20];
        char strOff[20];
        bool state = false;
        uint32_t msOn;
        uint32_t msOff;
        uint32_t timestampOn;
        uint32_t timestampOff;
};
