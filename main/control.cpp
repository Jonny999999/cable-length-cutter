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


//=============================
//========= readAdc ===========
//=============================
//function for multisampling an anlog input
int readAdc(adc1_channel_t adc_channel, bool inverted = false) {
    //make multiple measurements
    int adc_reading = 0;
    for (int i = 0; i < 16; i++) {
        adc_reading += adc1_get_raw(adc_channel);
    }
    adc_reading = adc_reading / 16;

    //return original or inverted result
    if (inverted) {
        return 4095 - adc_reading;
    } else {
        return adc_reading;
    }
}



//====================
//==== variables =====
//====================
static const char *TAG = "control";
char buf_disp[20]; //both displays
char buf_disp1[10];// 8 digits + decimal point + \0
char buf_disp2[10];// 8 digits + decimal point + \0
char buf_tmp[15];
max7219_t display; //display device
QueueHandle_t encoder_queue = NULL; //encoder event queue
rotary_encoder_info_t encoder; //encoder device/info
rotary_encoder_state_t encoderState;

uint8_t count = 0; //count for testing
uint32_t timestamp_pageSwitched = 0;
bool page = false; //store page number currently displayed
bool state = false; //store state of motor
int lengthNow = 0; //length measured in mm
int lengthTarget = 3000; //target length in mm
int potiRead = 0; //voltage read from adc

systemState_t controlState = COUNTING;

                      
           
//========================
//===== control task =====
//========================
void task_control(void *pvParameter)
{
    //initialize display
    display = init_display();
    //initialize encoder
//both dispencoder_queue = init_encoder(&encoder);

    //-----------------------------------
    //------- display welcome msg -------
    //-----------------------------------
    //display welcome message on 2 7 segment displays
    //show name and date
    ESP_LOGI(TAG, "showing startup message...");
    max7219_clear(&display);
    max7219_draw_text_7seg(&display, 0, "CUTTER  20.08.2022");
    //                                   1234567812 34 5678
    vTaskDelay(pdMS_TO_TICKS(700));
    //scroll "hello" over 2 displays
    for (int offset = 0; offset < 23; offset++) {
        max7219_clear(&display);
        char hello[23] = "                HELL0 ";
        max7219_draw_text_7seg(&display, 0, hello + (22 - offset) );
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    //================
    //===== loop =====
    //================
    while(1){
        vTaskDelay(20 / portTICK_PERIOD_MS);

        //-----------------------------
        //------ handle switches ------
        //-----------------------------
        //run handle functions for all switches
        SW_START.handle();
        SW_RESET.handle();
        SW_SET.handle();
        SW_PRESET1.handle();
        SW_PRESET2.handle();
        SW_PRESET3.handle();


        //----------------------------
        //------ rotary encoder ------
        //----------------------------
        // Poll current position and direction
        rotary_encoder_get_state(&encoder, &encoderState);
        //--- calculate distance ---
        //lengthNow = encoderState.position * (MEASURING_ROLL_DIAMETER * PI); //TODO dont calculate constant factor every time FIXME: ROUNDING ISSUE float-int?
        lengthNow += 155;


        //---------------------------
        //------ potentiometer ------
        //---------------------------
        //set target length to poti position when set switch is pressed
        if (SW_SET.state == true) {
            //read adc
            potiRead = readAdc(ADC_CHANNEL_POTI);
            //scale to target length range
            lengthTarget = (float)potiRead / 4095 * 50000;
            //round to whole meters
            lengthTarget = round(lengthTarget / 1000) * 1000;
            //TODO update lengthTarget only at button release?
        }
        //beep
        if (SW_SET.risingEdge) {
            buzzer.beep(1, 100, 50);
        }
        if (SW_SET.fallingEdge) {
            buzzer.beep(2, 100, 50);
        }


        //TODO: ADD PRESET SWITCHES HERE


        //--------------------------------
        //--------- statemachine ---------
        //--------------------------------
        switch (controlState) {
            case COUNTING:

                break;

            case WINDING_START:

                break;

            case WINDING:

                break;

            case TARGET_REACHED:

                break;
        }



        //---------------------------
        //--------- display ---------
        //---------------------------
        //-- show current position on display1 ---
        //sprintf(buf_tmp, "%06.1f cm", (float)lengthNow/10); //cm
        sprintf(buf_tmp, "1ST %5.4f", (float)lengthNow/1000); //m
        //                123456789
        //limit length to 8 digits + decimal point (drop decimal places when it does not fit)
        sprintf(buf_disp1, "%.9s", buf_tmp);

        //--- show target length on display2 ---
        //sprintf(buf_disp2, "%06.1f cm", (float)lengthTarget/10); //cm
        sprintf(buf_disp2, "S0LL%5.3f", (float)lengthTarget/1000); //m
        //                  1234  5678
        
        //TODO: blink disp2 when set button pressed

        //--- write to display ---
        //max7219_clear(&display); //results in flickering display if same value anyways
        max7219_draw_text_7seg(&display, 0, buf_disp1);
        max7219_draw_text_7seg(&display, 8, buf_disp2);

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

        //sprintf(display_buf, "S0LL 12.3");
        //max7219_draw_text_7seg(&display, 8, display_buf);


        //---------------------------
        //--------- testing ---------
        //---------------------------
        ////testing: rotate through speed levels
        //if(SW_SET.risingEdge){
        //    //rotate count 0-7
        //    if (count >= 7){
        //        count = 0;
        //    } else {
        //        count ++;
        //    }
        //    //set motor level
        //    vfd_setSpeedLevel(count);
        //    buzzer.beep(1, 100, 100);
        //}

        ////testing: toggle motor state
        //if(SW_START.risingEdge){
        //    state = !state;
        //    vfd_setState(state);
        //    buzzer.beep(1, 500, 100);
        //}
    }

}
