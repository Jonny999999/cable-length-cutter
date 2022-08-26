#include "vfd.hpp"

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1)

const char* vfd_directionStr[2] = {"FWD", "REV"};
static const char *TAG = "vfd";
uint8_t level = 0; //current speed level
bool state = false; //current state
vfd_direction_t direction = FWD; //current direction


//=============================
//========= setState ==========
//=============================
void vfd_setState(bool stateNew, vfd_direction_t directionNew){
    //only proceed and send log output when state or direction actually changed
    if ( state == stateNew && direction == directionNew ) {
        //already at target state and target direction -> do nothing
        return;
    }

    //log old and new state
    ESP_LOGI(TAG, "CHANGING vfd state from: %i %s", (int)state, vfd_directionStr[(int)direction]);
    ESP_LOGI(TAG, "CHANGING vfd state   to: %i %s", (int)stateNew, vfd_directionStr[(int)directionNew]);
    //update stored state
    state = stateNew;
    direction = directionNew;

    //turn motor on/off with target direction
    if (state == true) {
        switch (direction) {
            case FWD:
                gpio_set_level(GPIO_VFD_REV, 0);
                gpio_set_level(GPIO_VFD_FWD, 1);
                break;
            case REV:
                gpio_set_level(GPIO_VFD_FWD, 0);
                gpio_set_level(GPIO_VFD_REV, 1);
                break;
        }
        gpio_set_level(GPIO_RELAY, 1);
    } else {
        gpio_set_level(GPIO_VFD_FWD, 0);
        gpio_set_level(GPIO_VFD_REV, 0);
        gpio_set_level(GPIO_RELAY, 0);
    }
}



//=============================
//======= setSpeedLevel =======
//=============================
void vfd_setSpeedLevel(uint8_t levelNew){
    //set speed level of VFD

    //only proceed and send log output when level is actually changed
    if (level == levelNew) {
        //already at target level -> nothing todo
        return;
    }

    //log change
    ESP_LOGI(TAG, "CHANGING speed level from %i to %i", level, levelNew);
    //update stored level
    level = levelNew;

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
    if (level > 3) {
        level = 3;
    }

    //variables for logging the pin state
    //bool D0, D1, D2;
    bool D0, D1;

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

//    if CHECK_BIT(level, 2) {
//        D2 = true;
//        gpio_set_level(GPIO_VFD_D2, 1);        
//    } else {
//        D2 = false;
//        gpio_set_level(GPIO_VFD_D2, 0);        
//    }

    //log pin state
    //ESP_LOGI(TAG, " - pin state: D2=%i, D1=%i, D0=%i", (int)D2, (int)D1, (int)D0);
    ESP_LOGI(TAG, " - pin state: D1=%i, D0=%i", (int)D1, (int)D0);
}
