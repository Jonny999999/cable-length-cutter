#pragma once

#include <stdio.h>

extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
}

//constructor examples:
//switch to gnd and us internal pullup:
//gpio_evaluatedSwitch s3(GPIO_NUM_14);
//switch to gnd dont use internal pullup:
//gpio_evaluatedSwitch s3(GPIO_NUM_14 false);
//switch to VCC (inverted) and dont use internal pullup:
//gpio_evaluatedSwitch s3(GPIO_NUM_14 false, true);

enum class inputSource_t {GPIO, FUNCTION};

class gpio_evaluatedSwitch {
    public:
        //--- input ---
        uint32_t minOnMs = 30;
        uint32_t minOffMs = 30;

        //constructor minimal (default parameters pullup=true, inverted=false)
        gpio_evaluatedSwitch( 
                gpio_num_t gpio_num_declare
                );

        //constructor with optional parameters
        gpio_evaluatedSwitch(
                gpio_num_t gpio_num_declare,
                bool pullup_declare,
                bool inverted_declare=false
                );

        //constructor  with a function as source for input state instead of a gpio pin
        gpio_evaluatedSwitch(bool (*getInputStatePtr_f)(void), bool inverted_f=false); 


        //--- output ---         TODO make readonly? (e.g. public section: const int& x = m_x;)
        bool state = false;
        bool risingEdge = false;
        bool fallingEdge = false;
        uint32_t msPressed = 0;
        uint32_t msReleased = 0;

        //--- functions ---
        void handle();  //Statemachine for debouncing and edge detection

    private:
        gpio_num_t gpio_num;
        bool pullup;
        bool inverted;
        bool (*getInputStatePtr)(void); //pointer to function for getting current input state
        inputSource_t inputSource = inputSource_t::GPIO;

        enum class switchState {TRUE, FALSE, LOW, HIGH};
        switchState p_state = switchState::FALSE;
        bool inputState = false;
        uint32_t timestampLow = 0;
        uint32_t timestampHigh = 0;
        void initGpio();

};


