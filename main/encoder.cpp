extern "C" {
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_system.h"
#include "esp_log.h"
}

#include "encoder.hpp"


//----------------------------
//----- global variables -----
//----------------------------
rotary_encoder_info_t encoder; //encoder device/info
QueueHandle_t encoder_queue = NULL; //encoder event queue


//-------------------------
//------- functions -------
//-------------------------
//--- init_encoder ---
//initialize encoder and return event queue
QueueHandle_t init_encoder(rotary_encoder_info_t * info){
    // esp32-rotary-encoder requires that the GPIO ISR service is installed before calling rotary_encoder_register()
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    ESP_ERROR_CHECK(rotary_encoder_init(info, ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(info, ENABLE_HALF_STEPS));
#ifdef FLIP_DIRECTION
    ESP_ERROR_CHECK(rotary_encoder_flip_direction(info));
#endif

    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    QueueHandle_t event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(info, event_queue));
    return event_queue;
}
