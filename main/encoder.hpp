/* in this file all used functions from original rotary_encoder.h library are wrapped with custom functions to reduce global variables and duplicate code
*/
//TODO create a cpp class for an encoder?
#pragma once
extern "C" {
#include <freertos/task.h>
}

#include "config.hpp"


//----------------------------
//----- global variables -----
//----------------------------
//TODO ignore global encoder queue, since it is not used?
extern QueueHandle_t encoder_queue; //encoder event queue


//-------------------------
//------- functions -------
//-------------------------

//--- encoder_init ---
//init encoder
QueueHandle_t encoder_init();

//--- encoder_getSteps ---
//get steps counted since last reset
int encoder_getSteps();


//--- encoder_getLenMm ---
//get current length in Mm since last reset
int encoder_getLenMm();


    
//--- encoder_reset ---
//reset counted steps / length to 0
void encoder_reset();
