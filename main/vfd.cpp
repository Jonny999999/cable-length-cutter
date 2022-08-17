#include "vfd.hpp"

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

static const char *TAG = "vfd";



void vfd_setState(bool state){
    if (state == true) {
        gpio_set_level(GPIO_VFD_FWD, 1);
        gpio_set_level(GPIO_RELAY, 1);
    } else {
        gpio_set_level(GPIO_VFD_FWD, 0);
        gpio_set_level(GPIO_RELAY, 0);
    }
    ESP_LOGI(TAG, "set state to %i", (int)state);
}



void vfd_setSpeedLevel(uint8_t level){
    //limit to 7
    if (level > 7) {
        level = 7;
    }

    //bit:2  1  0
    //lvl D0 D1 D2
    //0   0  0  0
    //1   0  0  1
    //2   0  1  0
    //3   0  1  1
    //4   1  0  0
    //5   1  0  1
    //6   1  1  0
    //7   1  1  1

    //variables for logging the pin state
    bool D0, D1, D2;

    //set output state according to corresponding bit state
    if CHECK_BIT(level, 0) {
        D2 = true;
        gpio_set_level(GPIO_VFD_D2, 1);        
    } else {
        D2 = false;
        gpio_set_level(GPIO_VFD_D2, 0);        
    }
        
    if CHECK_BIT(level, 1) {
        D1 = true;
        gpio_set_level(GPIO_VFD_D1, 1);        
    } else {
        D1 = false;
        gpio_set_level(GPIO_VFD_D1, 0);        
    }

    if CHECK_BIT(level, 2) {
        D0 = true;
        gpio_set_level(GPIO_VFD_D0, 1);        
    } else {
        D0 = false;
        gpio_set_level(GPIO_VFD_D0, 0);        
    }

    //log
    ESP_LOGI(TAG, "Set level to %d", level);
    ESP_LOGI(TAG, "pin state: D0=%i, D1=%i, D2=%i", (int)D0, (int)D1, (int)D2);
}
