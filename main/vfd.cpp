#include "vfd.hpp"

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

static const char *TAG = "vfd";



void vfd_setState(bool state){
    //turn motor on/off
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
    //set speed level of VFD

    //bit:2  1  0
    //lvl D2 D1 D0  Hz
    //0   0  0  0   default
    //1   0  0  1   1
    //2   0  1  0   3
    //3   0  1  1   9
    //4   1  0  0   16
    //5   1  0  1   25
    //6   1  1  0   50
    //7   1  1  1   70

    //limit to 7
    if (level > 7) {
        level = 7;
    }

    //variables for logging the pin state
    bool D0, D1, D2;

    //set output state according to corresponding bit state
    if CHECK_BIT(level, 0) {
        D0 = true;
        gpio_set_level(GPIO_VFD_D0, 1);        
    } else {
        D0 = false;
        gpio_set_level(GPIO_VFD_D0, 0);        
    }
        
    if CHECK_BIT(level, 1) {
        D1 = true;
        gpio_set_level(GPIO_VFD_D1, 1);        
    } else {
        D1 = false;
        gpio_set_level(GPIO_VFD_D1, 0);        
    }

    if CHECK_BIT(level, 2) {
        D2 = true;
        gpio_set_level(GPIO_VFD_D2, 1);        
    } else {
        D2 = false;
        gpio_set_level(GPIO_VFD_D2, 0);        
    }

    //log
    ESP_LOGI(TAG, "Set level to %d", level);
    ESP_LOGI(TAG, "pin state: D2=%i, D1=%i, D0=%i", (int)D2, (int)D1, (int)D0);
}
