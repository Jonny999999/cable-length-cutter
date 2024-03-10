extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_system.h"
#include "esp_log.h"

#include "rotary_encoder.h"
}

#include "encoder.hpp"
#include "config.h"
#include "global.hpp"


//----------------------------
//----- global variables -----
//----------------------------
static rotary_encoder_info_t encoder; //encoder device/info
QueueHandle_t encoder_queue = NULL; //encoder event queue



//-------------------------
//------- functions -------
//-------------------------

//======================
//==== encoder_init ====
//======================
//initialize encoder and return event queue
QueueHandle_t encoder_init(){
    // esp32-rotary-encoder requires that the GPIO ISR service is installed before calling rotary_encoder_register()
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    ESP_ERROR_CHECK(rotary_encoder_init(&encoder, ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&encoder, ENABLE_HALF_STEPS));
#ifdef FLIP_DIRECTION
    ESP_ERROR_CHECK(rotary_encoder_flip_direction(&encoder));
#endif

    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    QueueHandle_t event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(&encoder, event_queue));
    return event_queue;
}


//========================
//=== encoder_getSteps ===
//========================
//get steps counted since last reset
int encoder_getSteps(){
    // Poll current position and direction
    rotary_encoder_state_t encoderState;
    rotary_encoder_get_state(&encoder, &encoderState);
    //calculate total distance since last reset
    return encoderState.position; 
}


//========================
//=== encoder_getLenMm ===
//========================
//get current length in Mm since last reset
int encoder_getLenMm(){
    return (float)encoder_getSteps() * 1000 / ENCODER_STEPS_PER_METER; 
}


//=======================
//==== encoder_reset ====
//=======================
//reset counted steps / length to 0
void encoder_reset(){
    rotary_encoder_reset(&encoder);
    return;
}    
