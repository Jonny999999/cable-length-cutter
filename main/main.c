#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_idf_version.h>
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

#include <max7219.h>
#include "rotary_encoder.h"

//--------------------------
//----- display config -----
//--------------------------
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
#define HOST    HSPI_HOST
#else
#define HOST    SPI2_HOST
#endif
#define DISPLAY_PIN_NUM_MOSI 23
#define DISPLAY_PIN_NUM_CLK 18
#define DISPLAY_PIN_NUM_CS 21
#define DISPLAY_DELAY 2000

//--------------------------
//----- encoder config -----
//--------------------------
#define TAG "app"
#define ROT_ENC_A_GPIO 16
#define ROT_ENC_B_GPIO 17
#define ENABLE_HALF_STEPS false  // Set to true to enable tracking of rotary encoder at half step resolution
#define FLIP_DIRECTION    false  // Set to true to reverse the clockwise/counterclockwise sense




void task(void *pvParameter)
{

    //========================
    //===== init encoder =====
    //========================
    // esp32-rotary-encoder requires that the GPIO ISR service is installed before calling rotary_encoder_register()
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    rotary_encoder_info_t info = { 0 };
    ESP_ERROR_CHECK(rotary_encoder_init(&info, ROT_ENC_A_GPIO, ROT_ENC_B_GPIO));
    ESP_ERROR_CHECK(rotary_encoder_enable_half_steps(&info, ENABLE_HALF_STEPS));
#ifdef FLIP_DIRECTION
    ESP_ERROR_CHECK(rotary_encoder_flip_direction(&info));
#endif

    // Create a queue for events from the rotary encoder driver.
    // Tasks can read from this queue to receive up to date position information.
    QueueHandle_t event_queue = rotary_encoder_create_queue();
    ESP_ERROR_CHECK(rotary_encoder_set_queue(&info, event_queue));




    //========================
    //===== init display =====
    //========================
    // Configure SPI bus
    spi_bus_config_t cfg = {
       .mosi_io_num = DISPLAY_PIN_NUM_MOSI,
       .miso_io_num = -1,
       .sclk_io_num = DISPLAY_PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    // Configure device
    max7219_t dev = {
       .cascade_size = 1,
       .digits = 8,
       .mirrored = true
    };
    ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, DISPLAY_PIN_NUM_CS));

    ESP_ERROR_CHECK(max7219_init(&dev));


    char buf[10]; // 8 digits + decimal point + \0

    //display startup message
        max7219_clear(&dev);
        max7219_draw_text_7seg(&dev, 0, "ABCDEFGH");
        printf("showing startup message...");
        vTaskDelay(pdMS_TO_TICKS(1000));


        //===== loop =====
        while (1)
        {


            vTaskDelay(pdMS_TO_TICKS(10)); //limit processing of events


            // Wait for incoming events on the event queue.
            rotary_encoder_event_t event = { 0 };
            if (xQueueReceive(event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE)
            {
                ESP_LOGI(TAG, "Event: position %d, direction %s", event.state.position,
                        event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");


                //--- show current position on display ---
                sprintf(buf, "%08d", event.state.position);
                //printf("float num\n");
                max7219_clear(&dev);
                max7219_draw_text_7seg(&dev, 0, buf);
            }
            else //no event for 1s
            {
                // Poll current position and direction
                rotary_encoder_state_t state = { 0 };
                ESP_ERROR_CHECK(rotary_encoder_get_state(&info, &state));
                ESP_LOGI(TAG, "Poll: position %d, direction %s", state.position,
                        state.direction ? (state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");

            }
        }
}






//display test
    //float count = 0;

//        printf("Display cycle\n");
//
//        max7219_clear(&dev);
//        max7219_draw_text_7seg(&dev, 0, "12345678");
//        printf("num");
//        vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY));
//
//        max7219_clear(&dev);
//        max7219_draw_text_7seg(&dev, 0, "ABCDEFGH");
//        printf("text");
//        vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY));
//
//
//        if (count > 20){
//            count = 0;
//        } else {
//            count += 0.005;
//        }
//
//        sprintf(buf, "%06.3f m", count);
//        //printf("float num\n");
//        max7219_clear(&dev);
//        max7219_draw_text_7seg(&dev, 0, buf);
//        vTaskDelay(pdMS_TO_TICKS(10));
//
   //     max7219_clear(&dev);
   //     sprintf(buf, "%08x", 12345678);
   //     printf("num\n");
   //     max7219_draw_text_7seg(&dev, 0, buf);
   //     vTaskDelay(pdMS_TO_TICKS(DISPLAY_DELAY));


//    }
//}

void app_main()
{
    xTaskCreate(task, "task", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
}


