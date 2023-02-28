#pragma once
extern "C" {
#include "rotary_encoder.h"
#include <freertos/task.h>
}

#include "config.hpp"


//----------------------------
//----- global variables -----
//----------------------------
extern rotary_encoder_info_t encoder; //encoder device/info
extern QueueHandle_t encoder_queue; //encoder event queue


//------------------------
//----- init encoder -----
//------------------------
QueueHandle_t init_encoder(rotary_encoder_info_t * info);
