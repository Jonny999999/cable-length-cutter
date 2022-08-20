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
static const char *TAG = "control"; //tag for logging

const char* systemStateStr[5] = {"COUNTING", "WINDING_START", "WINDING", "TARGET_REACHED", "MANUAL"};
systemState_t controlState = COUNTING;

max7219_t display; //display device
char buf_disp[20]; //both displays
char buf_disp1[10];// 8 digits + decimal point + \0
char buf_disp2[10];// 8 digits + decimal point + \0
char buf_tmp[15];

rotary_encoder_info_t encoder; //encoder device/info
QueueHandle_t encoder_queue = NULL; //encoder event queue
rotary_encoder_state_t encoderState;

uint8_t count = 0; //count for testing
uint32_t timestamp_pageSwitched = 0;
bool page = false; //store page number currently displayed
int lengthNow = 0; //length measured in mm
int lengthTarget = 3000; //target length in mm
int lengthRemaining = 0; //(target - now) length needed for reaching the target
int potiRead = 0; //voltage read from adc
uint32_t timestamp_motorStarted = 0; //timestamp winding started


//===== change State =====
//function for changing the controlState with log output
void changeState (systemState_t stateNew) {
    //only proceed when state actually changed
    if (controlState == stateNew){
        return; //already at target state -> nothing to do
    }
    //log change
    ESP_LOGW(TAG, "changed state from %s to %s", systemStateStr[(int)controlState], systemStateStr[(int)stateNew]);
    //change state
    controlState = stateNew;
}
                      

//===== handle Stop Condition =====
//function that checks whether start button is released or target is reached (used in multiple states)
//returns true when stopped, false when no action
bool handleStopCondition(){
    //--- stop conditions ---
    //stop conditions that are checked in any mode
    //target reached
    if (lengthRemaining <= 0 ) {
        changeState(TARGET_REACHED);
        vfd_setState(false);
        return true;
    }
    //start button released
    else if (SW_START.state == false) {
        changeState(COUNTING);
        vfd_setState(false);
        return true;
    } else {
        return false;
    }
}


//===== set dynamic speed level =====
//function that sets the vfd speed level dynamicly depending on the remaining distance
//closer to target -> slower
void setDynSpeedLvl(uint8_t lvlMax = 3){
    uint8_t lvl;
    //define speed level according to difference
    if (lengthRemaining < 50) {
        lvl = 0;
    } else if (lengthRemaining < 200) {
        lvl = 1;
    } else if (lengthRemaining < 600) {
        lvl = 2;
    } else { //more than last step remaining
        lvl = 3;
    }
    //limit to max lvl
    if (lvl > lvlMax){
        lvl = lvlMax;
    }
    //update vfd speed level
    vfd_setSpeedLevel(lvl);
}



           
//========================
//===== control task =====
//========================
void task_control(void *pvParameter)
{
    //initialize display
    display = init_display();
    //initialize encoder
    encoder_queue = init_encoder(&encoder);

    //-----------------------------------
    //------- display welcome msg -------
    //-----------------------------------
    //display welcome message on two 7 segment displays
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
        lengthNow = (float)encoderState.position * (MEASURING_ROLL_DIAMETER * PI) / 600; //TODO dont calculate constant factor every time FIXME: ROUNDING ISSUE float-int?



        //---------------------------
        //--------- buttons ---------
        //---------------------------
        //TODO: ADD PRESET SWITCHES HERE
        //--- RESET switch ---
        if (SW_RESET.risingEdge) {
            rotary_encoder_reset(&encoder);
            lengthNow = 0;
            buzzer.beep(1, 700, 100);
        }
       

        //--- manual mode ---
        //switch to manual motor control (2 buttons + poti)
        if ( SW_PRESET2.state && (SW_PRESET1.state || SW_PRESET3.state) && controlState != MANUAL ) {
            //enable manual control
            changeState(MANUAL);
            buzzer.beep(3, 100, 60);
        }


        //--- set custom target length ---
        //set target length to poti position when SET switch is pressed
        if (SW_SET.state == true) {
            //read adc
            potiRead = readAdc(ADC_CHANNEL_POTI); //0-4095
            //scale to target length range
            int lengthTargetNew = (float)potiRead / 4095 * 50000;
            //round to whole meters
            lengthTarget = round(lengthTarget / 1000) * 1000;
            //update target length and beep if changed
            if (lengthTargetNew != lengthTarget) {
                //TODO update lengthTarget only at button release?
                lengthTarget = lengthTargetNew;
                buzzer.beep(1, 60, 0);
            }
        }
        //beep start and end of editing
        if (SW_SET.risingEdge) {
            buzzer.beep(1, 70, 50);
        }
        if (SW_SET.fallingEdge) {
            buzzer.beep(2, 70, 50);
        }


        //--- target length presets ---
        if (controlState != MANUAL) { //dont apply preset length while controlling motor with preset buttons
            if (SW_PRESET1.risingEdge){
                lengthTarget = 1000;
                buzzer.beep(lengthTarget/1000, 50, 30);
            }
            else if (SW_PRESET2.risingEdge) {
                lengthTarget = 5000;
                buzzer.beep(lengthTarget/1000, 50, 30);
            }
            else if (SW_PRESET3.risingEdge) {
                lengthTarget = 10000;
                buzzer.beep(lengthTarget/1000, 50, 30);
            }
        }



        //---------------------------
        //--------- control ---------
        //---------------------------
        //calculate length difference
        lengthRemaining = lengthTarget - lengthNow;

        //--- statemachine ---
        switch (controlState) {
            case COUNTING: //no motor action
                vfd_setState(false);
                //--- start winding to length ---
                if (SW_START.risingEdge) {
                    changeState(WINDING_START);
                    //TODO apply dynamic speed here too (if started when already very close)
                    vfd_setSpeedLevel(1); //start at low speed 
                    vfd_setState(true); //start motor
                    timestamp_motorStarted = esp_log_timestamp(); //save time started
                    buzzer.beep(1, 100, 0);
                } 
                break;

            case WINDING_START: //wind slow for certain time
                //set vfd speed depending on remaining distance 
                setDynSpeedLvl(1); //limit to speed lvl 1 (force slow start)
                if (esp_log_timestamp() - timestamp_motorStarted > 2000) {
                    changeState(WINDING);
                }
                handleStopCondition(); //stops if button released or target reached
                //TODO: cancel when there was no cable movement during start time?
                break;

            case WINDING: //wind fast, slow down when close
                //set vfd speed depending on remaining distance 
                setDynSpeedLvl(); //slow down when close to target
                handleStopCondition(); //stops if button released or target reached
                //TODO: cancel when there is no cable movement anymore e.g. empty / timeout?
                break;

            case TARGET_REACHED:
                vfd_setState(false);
                //switch to counting state when no longer at or above target length
                if ( lengthRemaining > 0 ) {
                    changeState(COUNTING);
                }
                break;

            case MANUAL: //manually control motor via preset buttons + poti
                //read poti value
                potiRead = readAdc(ADC_CHANNEL_POTI); //0-4095
                //scale poti to speed levels 0-3
                uint8_t level = round( (float)potiRead / 4095 * 3 );
                //exit manual mode if preset2 released
                if ( SW_PRESET2.state == false ) {
                    changeState(COUNTING);
                    buzzer.beep(1, 1000, 100);
                }
                //P2 + P1 -> turn left
                else if ( SW_PRESET1.state && !SW_PRESET3.state ) {
                    vfd_setSpeedLevel(level); //TODO: use poti input for level
                    vfd_setState(true, REV);
                }
                //P2 + P3 -> turn right
                else if ( SW_PRESET3.state && !SW_PRESET1.state ) {
                    vfd_setSpeedLevel(level); //TODO: use poti input for level
                    vfd_setState(true, FWD);
                }
                //no valid switch combination -> turn off motor
                else {
                    vfd_setState(false);
                }
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
        //TODO: blink disp2 when preset button pressed (exept manual mode)
        //TODO: write "MAN CTL" to disp2 when in manual mode
        //TODO: display or blink "REACHED" when reached state and start pressed

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

    }

}
