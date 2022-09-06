#include "cutter.hpp"

const char* cutter_stateStr[5] = {"IDLE", "START", "CUTTING", "CANCELED", "TIMEOUT"}; //define strings for logging the state
                                                                          

//---------------------------
//----- local functions -----
//---------------------------
//declare local functions
void setState(cutter_state_t stateNew);
bool checkTimeout();



//---------------------------
//----- local variables -----
//---------------------------
cutter_state_t state = cutter_state_t::IDLE;
uint32_t timestamp_turnedOn;
uint32_t msTimeout = 3000;
static const char *TAG = "cutter"; //tag for logging




//=========================
//========= start =========
//=========================
void cutter_start(){
    setState(cutter_state_t::START);
    //starts motor on state change
}



//========================
//========= stop =========
//========================
void cutter_stop(){
    setState(cutter_state_t::CANCELED);
    //starts motor on state change
}



//===========================
//===== cutter_getState =====
//===========================
cutter_state_t cutter_getState(){
    return state;
}



//---------------------------
//-------- setState ---------
//---------------------------
//local function for changing state, taking corresponding actions and sending log output
void setState(cutter_state_t stateNew){
    //only proceed and send log output when state or direction actually changed
    if ( state == stateNew) {
        //already at target state -> do nothing
        return;
    }

    //log old and new state
    ESP_LOGI(TAG, "CHANGING state from: %i %s", (int)state, cutter_stateStr[(int)state]);
    ESP_LOGI(TAG, "CHANGING state   to: %i %s", (int)stateNew, cutter_stateStr[(int)stateNew]);
    //update stored state
    state = stateNew;

    switch(stateNew){
        case cutter_state_t::IDLE:
        case cutter_state_t::TIMEOUT:
        case cutter_state_t::CANCELED:
            //--- turn motor off ---
            gpio_set_level(GPIO_RELAY, 0);
            break;

        case cutter_state_t::START:
        case cutter_state_t::CUTTING:
            //--- turn motor on ---
            gpio_set_level(GPIO_RELAY, 1);
            //update state, timestamp
            timestamp_turnedOn = esp_log_timestamp();
            break;
    }
}



//--------------------------
//------ checkTimeout ------
//--------------------------
//local function that checks for timeout
bool checkTimeout(){
    if (esp_log_timestamp() - timestamp_turnedOn > msTimeout){
        setState(cutter_state_t::TIMEOUT);
        return true;
    } else {
        return false;
    }
}




//========================
//======== handle ========
//========================
//function that handles the cutter logic -> has to be run repeatedly
void cutter_handle(){

    switch(state){
        case cutter_state_t::IDLE:
        case cutter_state_t::TIMEOUT:
        case cutter_state_t::CANCELED:
            //wait for state change via cutter_start();
            break;

        case cutter_state_t::START:
            //--- moved away from idle position ---
            if (gpio_get_level(GPIO_CUTTER_POS_SW) == 0){ //switch closed
                                                          //FIXME: initialize gpio as input with PULLUP somewhere
                setState(cutter_state_t::CUTTING);
            }
            //--- timeout ---
            else {
                checkTimeout();
            }

            break;

        case cutter_state_t::CUTTING:
            //--- idle position reached ---
            if (gpio_get_level(GPIO_CUTTER_POS_SW) == 1){ //switch not closed
                setState(cutter_state_t::IDLE);
            }
            //--- timeout ---
            else {
                checkTimeout();
            }
            break;
    }
}

