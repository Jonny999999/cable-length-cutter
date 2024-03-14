#include "cutter.hpp"
#include "config.h"
#include "global.hpp"

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
static cutter_state_t cutter_state = cutter_state_t::IDLE;
static uint32_t timestamp_turnedOn;
static uint32_t msTimeout = 3000;
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
    if(cutter_state != cutter_state_t::IDLE){
        setState(cutter_state_t::CANCELED);
    }
}



//===========================
//===== cutter_getState =====
//===========================
cutter_state_t cutter_getState(){
    return cutter_state;
}



//============================
//===== cutter_isRunning =====
//============================
bool cutter_isRunning(){
    if (cutter_state == cutter_state_t::START 
            || cutter_state == cutter_state_t::CUTTING) {
        return true;
    } else {
        return false;
    }
}



//---------------------------
//-------- setState ---------
//---------------------------
//local function for changing state, taking corresponding actions and sending log output
void setState(cutter_state_t stateNew){
    //only proceed and send log output when state or direction actually changed
    if ( cutter_state == stateNew) {
        //already at target state -> do nothing
        return;
    }

    //log old and new state
    ESP_LOGI(TAG, "CHANGING state from: %s",cutter_stateStr[(int)cutter_state]);
    ESP_LOGI(TAG, "CHANGING state   to: %s",cutter_stateStr[(int)stateNew]);
    //update stored state
    cutter_state = stateNew;

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
    //handle evaluated switch (position switch)
    SW_CUTTER_POS.handle();
    //TODO add custom thresholds once at initialization?
    //SW_CUTTER_POS.minOnMs = 10;
    //SW_CUTTER_POS.minOffMs = 10;

    switch(cutter_state){
        case cutter_state_t::IDLE:
        case cutter_state_t::TIMEOUT:
        case cutter_state_t::CANCELED:
            //wait for state change via cutter_start();
            break;

        case cutter_state_t::START:
            //--- moved away from idle position ---
            //if (gpio_get_level(GPIO_CUTTER_POS_SW) == 0){ //contact closed
            if (SW_CUTTER_POS.state == true) { //contact closed -> not at idle pos anymore
                setState(cutter_state_t::CUTTING);
            }
            //--- timeout ---
            else {
                checkTimeout();
            }
            break;

        case cutter_state_t::CUTTING:
            //--- idle position reached ---
            //if (gpio_get_level(GPIO_CUTTER_POS_SW) == 1){ //contact not closed
            //TODO: add min on duration
            if (SW_CUTTER_POS.state == false) { //contact open -> at idle pos
                setState(cutter_state_t::IDLE);
            }
            //--- timeout ---
            else {
                checkTimeout();
            }
            break;
    }
}

