#include "config.hpp"


//--- inputs ---
//create and configure objects for evaluated switches
gpio_evaluatedSwitch SW_START(GPIO_SW_START, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch SW_RESET(GPIO_SW_RESET, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch SW_SET(GPIO_SW_SET, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch SW_PRESET1(GPIO_SW_PRESET1, true, false); //pullup true, not inverted (switch to GND, internal pullup used)
gpio_evaluatedSwitch SW_PRESET2(GPIO_SW_PRESET2, false, true); //pullup false, INVERTED (switch to 3V3, pulldown on pcb soldered)
gpio_evaluatedSwitch SW_PRESET3(GPIO_SW_PRESET3, false, true); //pullup false, INVERTED (switch to 3V3, pulldown on pcb soldered)


//create buzzer object with no gap between beep events
buzzer_t buzzer(GPIO_BUZZER, 0);
