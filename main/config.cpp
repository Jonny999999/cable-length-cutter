#include "config.hpp"


//--- inputs ---
//create objects for switches at bottom screw temerinals
gpio_evaluatedSwitch SW_START(GPIO_SW_START, true, false); //pullup true, not inverted (switch to GND, pullup on receiver pcb)
gpio_evaluatedSwitch SW_RESET(GPIO_SW_RESET, true, false); //pullup true, not inverted (switch to GND, pullup on receiver pcb)
gpio_evaluatedSwitch SW_SET(GPIO_SW_SET, true, false); //pullup true, not inverted (switch to GND, pullup on receiver pcb)
gpio_evaluatedSwitch SW_PRESET1(GPIO_SW_PRESET1, true, false); //pullup true, not inverted (switch to GND, pullup on receiver pcb)
gpio_evaluatedSwitch SW_PRESET2(GPIO_SW_PRESET2, true, false); //pullup true, not inverted (switch to GND, pullup on receiver pcb)
gpio_evaluatedSwitch SW_PRESET3(GPIO_SW_PRESET3, true, false); //pullup true, not inverted (switch to GND, pullup on receiver pcb)


//create buzzer object with gap between queued events of 100ms 
buzzer_t buzzer(GPIO_BUZZER, 100);
