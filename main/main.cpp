extern "C"
{
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_idf_version.h>
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"

#include <max7219.h>
#include "rotary_encoder.h"
}

#include "gpio_evaluateSwitch.hpp"
#include "config.hpp"
#include "control.hpp"
#include "buzzer.hpp"



//function to configure gpio pin as output
void gpio_configure_output(gpio_num_t gpio_pin){
    gpio_pad_select_gpio(gpio_pin);
    gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
}


void init_gpios(){
    //initialize all outputs
    //4x stepper mosfets
    gpio_configure_output(GPIO_VFD_FWD);
    gpio_configure_output(GPIO_VFD_D0);
    gpio_configure_output(GPIO_VFD_D1);
    gpio_configure_output(GPIO_VFD_D2);
    //2x power mosfets
    gpio_configure_output(GPIO_MOS1);
    gpio_configure_output(GPIO_MOS2);
    //onboard relay and buzzer
    gpio_configure_output(GPIO_RELAY);
    gpio_configure_output(GPIO_BUZZER);
    //5v regulator
    gpio_configure_output(GPIO_NUM_17);
}



//task for testing the encoder and display
void task_testing(void *pvParameter)
{

    //========================
    //===== init encoder =====
    //========================
    // esp32-rotary-encoder requires that the GPIO ISR service is installed before calling rotary_encoder_register()
    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    // Initialise the rotary encoder device with the GPIOs for A and B signals
    rotary_encoder_info_t info;
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
       dev.cascade_size = 1;
       dev.digits = 8;
       dev.mirrored = true;
    ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, DISPLAY_PIN_NUM_CS));

    ESP_ERROR_CHECK(max7219_init(&dev));

    //0...15
    ESP_ERROR_CHECK(max7219_set_brightness(&dev, 12));



    char buf[10]; // 8 digits + decimal point + \0
    int32_t distance_mm = 0;

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
            rotary_encoder_event_t event;
            if (xQueueReceive(event_queue, &event, 1000 / portTICK_PERIOD_MS) == pdTRUE)
            {
                ESP_LOGI(TAG, "Event: position %d, direction %s", event.state.position,
                        event.state.direction ? (event.state.direction == ROTARY_ENCODER_DIRECTION_CLOCKWISE ? "CW" : "CCW") : "NOT_SET");


                //--- calculate distalce ---
                distance_mm = event.state.position * (MEASURING_ROLL_DIAMETER * PI / 600); //TODO dont calculate constant factor every time
                //--- show current position on display ---
                //sprintf(buf, "%08d", event.state.position);
                //--- show current distance in cm on display ---
                sprintf(buf, "%06.1f cm", (float)distance_mm/10);
                //printf("float num\n");
                max7219_clear(&dev);
                max7219_draw_text_7seg(&dev, 0, buf);
            }
            else //no event for 1s
            {
                // Poll current position and direction
                rotary_encoder_state_t state;
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


//======================================
//============ buzzer task =============
//======================================
//TODO: move the task creation to buzzer class (buzzer.cpp)
//e.g. only have function buzzer.createTask() in app_main
void task_buzzer( void * pvParameters ){
    ESP_LOGI("task_buzzer", "Start of buzzer task...");
        //run function that waits for a beep events to arrive in the queue
        //and processes them
        buzzer.processQueue();
}






extern "C" void app_main()
{

    //init outputs
    init_gpios();
    //enable 5V volate regulator
    gpio_set_level(GPIO_NUM_17, 1);


    //--- create task testing ---
    xTaskCreate(task_testing, "task_testing", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    //--- create task control ---
    xTaskCreate(task_control, "task_control", configMINIMAL_STACK_SIZE * 3, NULL, 5, NULL);
    //--- create task for buzzer ---
    xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 2, NULL);

    //beep at startup
    buzzer.beep(3, 70, 50);
}
