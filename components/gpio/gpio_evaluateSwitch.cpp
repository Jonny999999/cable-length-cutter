#include "gpio_evaluateSwitch.hpp"

static const char *TAG = "evaluateSwitch";


gpio_evaluatedSwitch::gpio_evaluatedSwitch( //minimal (use default values)
        gpio_num_t gpio_num_declare
        ){
    gpio_num = gpio_num_declare;
    pullup = true;
    inverted = false;
    inputSource = inputSource_t::GPIO;

    initGpio();
};


gpio_evaluatedSwitch::gpio_evaluatedSwitch( //optional parameters given
        gpio_num_t gpio_num_declare,
        bool pullup_declare,
        bool inverted_declare
        ){
    gpio_num = gpio_num_declare;
    pullup = pullup_declare;
    inverted = inverted_declare;
    inputSource = inputSource_t::GPIO;

    initGpio();
};


gpio_evaluatedSwitch::gpio_evaluatedSwitch( //with function as input source
        bool (*getInputStatePtr_f)(void),
        bool inverted_f){
    //gpio_num = NULL;
    //pullup = NULL;
    inverted = inverted_f;
    getInputStatePtr = getInputStatePtr_f;
    inputSource = inputSource_t::FUNCTION;
};



void gpio_evaluatedSwitch::initGpio(){
    ESP_LOGI(TAG, "initializing gpio pin %d", (int)gpio_num);

    //define gpio pin as input
    gpio_pad_select_gpio(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);

    if (pullup == true){ //enable pullup if desired (default)
        gpio_pad_select_gpio(gpio_num);
        gpio_set_pull_mode(gpio_num, GPIO_PULLUP_ONLY);
    }else{
        gpio_set_pull_mode(gpio_num, GPIO_FLOATING);
        gpio_pad_select_gpio(gpio_num);
    }
    //TODO add pulldown option
    //gpio_set_pull_mode(gpio_num, GPIO_PULLDOWN_ONLY);
};



void gpio_evaluatedSwitch::handle(){  //Statemachine for debouncing and edge detection

    //--- get pin state with required method ---
    switch (inputSource){
        case inputSource_t::GPIO: //from gpio pin
            if (gpio_get_level(gpio_num) == 0){ //pin low
                inputState = true;
            } else { //pin high
                inputState = false;
            }
            break;
        case inputSource_t::FUNCTION: //from funtion
            inputState = (*getInputStatePtr)();
            break;
    }

    //--- invert state ---
    //not inverted: switch switches to GND when active
    //inverted: switch switched to VCC when active
    if (inverted == true){
        inputState = !inputState;
    }


    //=========================================================
    //========= Statemachine for evaluateing switch ===========
    //=========================================================
    switch (p_state){
        default:
            p_state = switchState::FALSE;
            break;

        case switchState::FALSE:  //input confirmed high (released)
            fallingEdge = false; //reset edge event
            if (inputState == true){ //pin high (on)
                p_state = switchState::HIGH;
                timestampHigh = esp_log_timestamp(); //save timestamp switched from low to high
            } else {
                msReleased = esp_log_timestamp() - timestampLow; //update duration released
            }
            break;

        case switchState::HIGH: //input recently switched to high (pressed)
            if (inputState == true){ //pin still high (on)
                if (esp_log_timestamp() - timestampHigh > minOnMs){ //pin in same state long enough
                    p_state = switchState::TRUE;
                    state = true;
                    risingEdge = true;
                    msReleased = timestampHigh - timestampLow; //calculate duration the button was released 
                }
            }else{
                p_state = switchState::FALSE;
            }
            break;

        case switchState::TRUE:  //input confirmed high (pressed)
            risingEdge = false; //reset edge event
            if (inputState == false){ //pin low (off)
                timestampLow = esp_log_timestamp();
                p_state = switchState::LOW;
            } else {
                msPressed = esp_log_timestamp() - timestampHigh; //update duration pressed
            }

            break;

        case switchState::LOW: //input recently switched to low (released)
            if (inputState == false){ //pin still low (off)
                if (esp_log_timestamp() - timestampLow > minOffMs){ //pin in same state long enough
                    p_state = switchState::FALSE;
                    msPressed = timestampLow - timestampHigh; //calculate duration the button was pressed
                    state=false;
                    fallingEdge=true;
                }
            }else{
                p_state = switchState::TRUE;
            }
            break;
    }

}


