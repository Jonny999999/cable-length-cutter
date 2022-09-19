#pragma once

extern "C"
{
#include <stdio.h>
#include "esp_log.h"
}

#include "buzzer.hpp"
#include "display.hpp"
#include "gpio_evaluateSwitch.hpp"


//--- variables ---
//enum for state of cutter
enum class cutter_state_t {IDLE, START, CUTTING, CANCELED, TIMEOUT};
//string for logging the state name
extern const char* cutter_stateStr[5];



//--- functions ---
//trigger cut cycle (no effect if already running)
void cutter_start();

//cancel cutting action
void cutter_stop();

//return current state
cutter_state_t cutter_getState();
//TODO: bool cutter_isOn() (simply return boolean instead of enum)

//check if cutter is currently operating
bool cutter_isRunning();

//handle function - has to be run repeatedly
void cutter_handle();
