extern "C"
{
#include <stdio.h>
#include "esp_log.h"
}

#include "buzzer.hpp"
#include "display.hpp"


//--- variables ---
//enum for state of cutter
typedef enum cutter_state_t {IDLE, START, CUTTING, CANCELED, TIMEOUT} cutter_state_t;
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

//handle function - has to be run repeatedly
void cutter_handle();
