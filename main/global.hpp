#pragma once

extern "C" {
#include "driver/adc.h"
}
#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "switchesAnalog.hpp"

//note: in the actual code macro variables to these objects from config.h are used as the objects names

//============================
//===== global variables =====
//============================
//create global evaluated switch objects for all available pins
//--- switches on digital gpio pins ---
//extern gpio_evaluatedSwitch sw_gpio_39;
extern gpio_evaluatedSwitch sw_gpio_34;
extern gpio_evaluatedSwitch sw_gpio_32;
extern gpio_evaluatedSwitch sw_gpio_33;
extern gpio_evaluatedSwitch sw_gpio_25;
extern gpio_evaluatedSwitch sw_gpio_26;
extern gpio_evaluatedSwitch sw_gpio_14;

//--- switches connected to 4-sw-to-analog stripboard ---
extern gpio_evaluatedSwitch sw_gpio_analog_0;
extern gpio_evaluatedSwitch sw_gpio_analog_1;
extern gpio_evaluatedSwitch sw_gpio_analog_2;
extern gpio_evaluatedSwitch sw_gpio_analog_3;


//create global buzzer object
extern buzzer_t buzzer;