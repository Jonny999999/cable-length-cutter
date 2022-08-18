#include "control.hpp"

//========================
//===== init display =====
//========================
max7219_t init_display(){
    // Configure SPI bus
    spi_bus_config_t cfg;
    cfg.mosi_io_num = DISPLAY_PIN_NUM_MOSI;
    cfg.miso_io_num = -1;
    cfg.sclk_io_num = DISPLAY_PIN_NUM_CLK;
    cfg.quadwp_io_num = -1;
    cfg.quadhd_io_num = -1;
    cfg.max_transfer_sz = 0;
    cfg.flags = 0;
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    // Configure device
    max7219_t dev;
    dev.cascade_size = 2;
    dev.digits = 0;
    dev.mirrored = true;
    ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, DISPLAY_PIN_NUM_CS));
    ESP_ERROR_CHECK(max7219_init(&dev));
    //0...15
    ESP_ERROR_CHECK(max7219_set_brightness(&dev, 12));
    return dev;
}


//========================
//===== init encoder =====
//========================
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


//============================
//===== display distance =====
//============================
//display current position in meters from current encoder count
void display_current_distance(max7219_t * dev, rotary_encoder_info_t * info){
    // --- event based action ---
    // Wait for incoming events on the event queue.
    // rotary_encoder_event_t event;
    // if (xQueueReceive(event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE)
    // {
    //     ESP_LOGI(TAG, "Event: position %d, direction %s", event.state.position,
    //             event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");
    //     //--- calculate distalce ---
    //     distance_mm = event.state.position * (MEASURING_ROLL_DIAMETER * PI / 600); //TODO dont calculate constant factor every time
    //     //--- show current position on display ---
    //     //sprintf(buf, "%08d", event.state.position);
    //     //--- show current distance in cm on display ---
    //     sprintf(buf, "%06.1f cm", (float)distance_mm/10);
    //     //printf("float num\n");
    //     max7219_clear(&dev);
    //     max7219_draw_text_7seg(&dev, 0, buf);
    // }
    // else //no event for 1s
    // {
    //     // Poll current position and direction
    //     rotary_encoder_state_t state;
    //     ESP_ERROR_CHECK(rotary_encoder_get_state(&info, &state));
    //     ESP_LOGI(TAG, "Poll: position %d, direction %s", state.position,
    //             state.direction ? (state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");
    // }

    // Poll current position and direction
    rotary_encoder_state_t state;
    rotary_encoder_get_state(info, &state);
    //--- calculate distalce ---
    float distance_mm = state.position * (MEASURING_ROLL_DIAMETER * PI / 600); //TODO dont calculate constant factor every time

    //--- show current position on display ---
    char buf[10]; // 8 digits + decimal point + \0
    sprintf(buf, "%06.1f cm", (float)distance_mm/10);
    //printf("float num\n");
    max7219_clear(dev);
    max7219_draw_text_7seg(dev, 0, buf);
}




//====================
//==== variables =====
//====================
static const char *TAG = "control";
char display_buf[20]; // 8 digits + decimal point + \0
max7219_t display; //display device
QueueHandle_t encoder_queue = NULL; //encoder event queue
rotary_encoder_info_t encoder; //encoder device/info

uint8_t count = 0; //count for testing
uint32_t timestamp_pageSwitched = 0;
bool page = false; //store page number currently displayed
bool state = false; //store state of motor

                      
           
//========================
//===== control task =====
//========================
void task_control(void *pvParameter)
{
    //initialize display
    display = init_display();
    //initialize encoder
    encoder_queue = init_encoder(&encoder);

    //---- startup message ----
    //display welcome message on 7 segment display
    ESP_LOGI(TAG, "showing startup message...");
    max7219_clear(&display);
    max7219_draw_text_7seg(&display, 0, "CUTTER  20.08.2022");
    //                                   1234567812 34 5678
    vTaskDelay(pdMS_TO_TICKS(700));
    for (int offset = 0; offset < 23; offset++) {
        max7219_clear(&display);
        char hello[23] = "                HELL0 ";
        max7219_draw_text_7seg(&display, 0, hello + (22 - offset) );
        vTaskDelay(pdMS_TO_TICKS(50));
    }

        max7219_clear(&display);
            sprintf(display_buf, "1ST 987.4");
            max7219_draw_text_7seg(&display, 0, display_buf);

    //===== loop =====
    while(1){
        vTaskDelay(20 / portTICK_PERIOD_MS);

        //run handle functions for all switches
        SW_START.handle();
        SW_RESET.handle();
        SW_SET.handle();
        SW_PRESET1.handle();
        SW_PRESET2.handle();
        SW_PRESET3.handle();

      //  //switch between two display pages
      //  if (esp_log_timestamp() - timestamp_pageSwitched > 1000){
      //      timestamp_pageSwitched = esp_log_timestamp();
      //      page = !page;
      //  }
      //  max7219_clear(&display);
      //  if (page){
      //      //display current position
      //      display_current_distance(&display, &encoder);
      //  } else {
      //      //display counter
      //      sprintf(display_buf, "lvl: %02d", count);
      //      max7219_draw_text_7seg(&display, 0, display_buf);
      //      //count++;
      //  }
            sprintf(display_buf, "S0LL 12.3");
            max7219_draw_text_7seg(&display, 8, display_buf);


        //testing: rotate through speed levels
        if(SW_SET.risingEdge){
            //rotate count 0-7
            if (count >= 7){
                count = 0;
            } else {
                count ++;
            }
            //set motor level
            vfd_setSpeedLevel(count);
            buzzer.beep(1, 100, 100);
        }


        //testing: toggle motor state
        if(SW_START.risingEdge){
            state = !state;
            vfd_setState(state);
            buzzer.beep(1, 500, 100);
        }
    }

}
