#include "config.hpp"


//--- inputs ---
//create and configure objects for evaluated switches
//gpio_evaluatedSwitch sw_gpio_39(GPIO_NUM_39, false, true); //pullup false, INVERTED (switch to 3V3, pulldown on pcb soldered)
gpio_evaluatedSwitch sw_gpio_34(GPIO_NUM_34, false, true); //pullup false, INVERTED (switch to 3V3, pulldown on pcb soldered)
gpio_evaluatedSwitch sw_gpio_32(GPIO_NUM_32, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch sw_gpio_33(GPIO_NUM_33, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch sw_gpio_25(GPIO_NUM_25, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch sw_gpio_26(GPIO_NUM_26, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch sw_gpio_14(GPIO_NUM_14, true, false); //pullup true, not inverted (switch to GND, internal pullup used)

//--- switches connected to 4 sw to analog stripboard ---
//evaluated switches with function to obtain the current input state instead of gpio
gpio_evaluatedSwitch sw_gpio_analog_0(&switchesAnalog_getState_sw0);
gpio_evaluatedSwitch sw_gpio_analog_1(&switchesAnalog_getState_sw1);
gpio_evaluatedSwitch sw_gpio_analog_2(&switchesAnalog_getState_sw2);
gpio_evaluatedSwitch sw_gpio_analog_3(&switchesAnalog_getState_sw3);

//create buzzer object with no gap between beep events
buzzer_t buzzer(GPIO_BUZZER, 0);
