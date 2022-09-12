#include "control.hpp"


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




//====================
//==== variables =====
//====================
static const char *TAG = "control"; //tag for logging

const char* systemStateStr[5] = {"COUNTING", "WINDING_START", "WINDING", "TARGET_REACHED", "MANUAL"};
systemState_t controlState = COUNTING;

char buf_disp[20]; //both displays
char buf_disp1[10];// 8 digits + decimal point + \0
char buf_disp2[10];// 8 digits + decimal point + \0
char buf_tmp[15];

rotary_encoder_info_t encoder; //encoder device/info
QueueHandle_t encoder_queue = NULL; //encoder event queue
rotary_encoder_state_t encoderState;

uint32_t timestamp_pageSwitched = 0;
bool page = false; //store page number currently displayed
int lengthNow = 0; //length measured in mm
int lengthTarget = 3000; //target length in mm
int lengthRemaining = 0; //(target - now) length needed for reaching the target
int potiRead = 0; //voltage read from adc
uint32_t timestamp_motorStarted = 0; //timestamp winding started
                                     
int lengthBeeped = 0; //only beep once per meter during encoder test


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
bool handleStopCondition(handledDisplay * displayTop, handledDisplay * displayBot){
    //--- stop conditions ---
    //stop conditions that are checked in any mode
    //target reached
    if (lengthRemaining <= 0 ) {
        changeState(TARGET_REACHED);
        vfd_setState(false);
        displayTop->blink(1, 0, 1500, "  S0LL  ");
        displayBot->blink(1, 0, 1500, "ERREICHT");
        buzzer.beep(2, 100, 100);
        return true;
    }
    //start button released
    else if (SW_START.state == false) {
        changeState(COUNTING);
        vfd_setState(false);
        displayTop->blink(2, 900, 1000, "- STOP -");
        displayBot->blink(2, 900, 1000, " TASTER ");
        buzzer.beep(3, 200, 100);
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
    //initialize encoder
    encoder_queue = init_encoder(&encoder);

    //initialize display
    max7219_t two7SegDisplays = display_init();
    //create two separate handled display instances
    handledDisplay displayTop(two7SegDisplays, 0);
    handledDisplay displayBot(two7SegDisplays, 8);

    //--- display welcome msg ---
    //display welcome message on two 7 segment displays
    //currently show name and date and scrolling 'hello'
    display_ShowWelcomeMsg(two7SegDisplays);


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
        //lengthNow = (float)encoderState.position * (MEASURING_ROLL_DIAMETER * PI) / 600; //TODO dont calculate constant factor every time FIXME: ROUNDING ISSUE float-int?
        lengthNow = (float)encoderState.position * 1000 / STEPS_PER_METER;



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
            potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095
            //scale to target length range
            int lengthTargetNew = (float)potiRead / 4095 * 30000;
            //apply hysteresis and round to whole meters //TODO optimize this
            if (lengthTargetNew % 1000 < 200) { //round down if less than .2 meter
                ESP_LOGD(TAG, "Poti input = %d -> rounding down", lengthTargetNew);
                lengthTargetNew = (lengthTargetNew/1000 ) * 1000; //round down
            } else if (lengthTargetNew % 1000 > 800 ) { //round up if more than .8 meter
                ESP_LOGD(TAG, "Poti input = %d -> rounding up", lengthTargetNew);
                lengthTargetNew = (lengthTargetNew/1000 + 1) * 1000; //round up
            } else {
                ESP_LOGD(TAG, "Poti input = %d -> hysteresis", lengthTargetNew);
                lengthTargetNew = lengthTarget;
            }
            //update target length and beep when effectively changed
            if (lengthTargetNew != lengthTarget) {
                //TODO update lengthTarget only at button release?
                lengthTarget = lengthTargetNew;
                ESP_LOGI(TAG, "Changed target length to %d mm", lengthTarget);
                buzzer.beep(1, 25, 10);
            }
        }
        //beep start and end of editing
        if (SW_SET.risingEdge) {
            buzzer.beep(1, 70, 50);
        }
        if (SW_SET.fallingEdge) {
            buzzer.beep(2, 70, 50);
            displayBot.blink(2, 100, 100, "S0LL    ");
        }


        //--- target length presets ---
        if (controlState != MANUAL) { //dont apply preset length while controlling motor with preset buttons
            if (SW_PRESET1.risingEdge){
                lengthTarget = 1000;
                buzzer.beep(lengthTarget/1000, 25, 30);
                displayBot.blink(3, 100, 100, "S0LL    ");
            }
            else if (SW_PRESET2.risingEdge) {
                lengthTarget = 5000;
                buzzer.beep(lengthTarget/1000, 25, 30);
                displayBot.blink(2, 100, 100, "S0LL    ");
            }
            else if (SW_PRESET3.risingEdge) {
                lengthTarget = 10000;
                buzzer.beep(lengthTarget/1000, 25, 30);
                displayBot.blink(2, 100, 100, "S0LL    ");
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
                //TODO check stop condition before starting - prevents motor from starting 2 cycles when 
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
                handleStopCondition(&displayTop, &displayBot); //stops if button released or target reached
                //TODO: cancel when there was no cable movement during start time?
                break;

            case WINDING: //wind fast, slow down when close
                //set vfd speed depending on remaining distance 
                setDynSpeedLvl(); //slow down when close to target
                handleStopCondition(&displayTop, &displayBot); //stops if button released or target reached
                //TODO: cancel when there is no cable movement anymore e.g. empty / timeout?
                break;

            case TARGET_REACHED:
                vfd_setState(false);
                //switch to counting state when no longer at or above target length
                if ( lengthRemaining > 0 ) {
                    changeState(COUNTING);
                }
                //show msg when trying to start, but target is reached
                if (SW_START.risingEdge){
                    buzzer.beep(3, 40, 30);
                    displayTop.blink(2, 600, 800, "  S0LL  ");
                    displayBot.blink(2, 600, 800, "ERREICHT");
                }
                break;

            case MANUAL: //manually control motor via preset buttons + poti
                //read poti value
                potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095
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
                    sprintf(buf_disp2, "[--%02i   ", level);
                    //                  123 45 678
                }
                //P2 + P3 -> turn right
                else if ( SW_PRESET3.state && !SW_PRESET1.state ) {
                    vfd_setSpeedLevel(level); //TODO: use poti input for level
                    vfd_setState(true, FWD);
                    sprintf(buf_disp2, "   %02i--]", level);
                }
                //no valid switch combination -> turn off motor
                else {
                    vfd_setState(false);
                    sprintf(buf_disp2, "   %02i   ", level);
                }
        }



        //--------------------------
        //------ encoder test ------
        //--------------------------
#ifdef ENCODER_TEST
        //run display handle functions
        displayTop.handle();
        displayBot.handle();
        //-- show encoder steps on display1 ---
        sprintf(buf_disp1, "EN %05d", encoderState.position); //count
        displayTop.showString(buf_disp1);
        //--- show converted distance on display2 ---
        sprintf(buf_disp2, "Met %5.3f", (float)lengthNow/1000); //m
        displayBot.showString(buf_disp2);
        //--- beep every 1m ---
        //note: only works precicely in forward/positive direction
        if (lengthNow % 1000 < 50) { //with tolerance in case of missed exact value
            if (fabs(lengthNow - lengthBeeped) >= 900) { //dont beep multiple times at same meter
                //TODO: add case for reverse direction. currently beeps 0.1 too early
                buzzer.beep(1, 400, 100 );
                lengthBeeped = lengthNow;
            }
        }
#else

        //--------------------------
        //-------- display1 --------
        //--------------------------
        //run handle function
        displayTop.handle();
        //show current position on display
        sprintf(buf_tmp, "1ST %5.4f", (float)lengthNow/1000);
        //                123456789
        //limit length to 8 digits + decimal point (drop decimal places when it does not fit)
        sprintf(buf_disp1, "%.9s", buf_tmp);
        displayTop.showString(buf_disp1);


        //--------------------------
        //-------- display2 --------
        //--------------------------
        //run handle function
        displayBot.handle();
        //setting target length: blink target length
        if (SW_SET.state == true){
            sprintf(buf_tmp, "S0LL%5.3f", (float)lengthTarget/1000);
            displayBot.blinkStrings(buf_tmp, "S0LL    ", 300, 100);
        }
        //manual state: blink "manual"
        else if (controlState == MANUAL) {
            displayBot.blinkStrings(" MANUAL ", buf_disp2, 1000, 1000);
        }
        //otherwise show target length
        else {
            //sprintf(buf_disp2, "%06.1f cm", (float)lengthTarget/10); //cm
            sprintf(buf_tmp, "S0LL%5.3f", (float)lengthTarget/1000); //m
            //                1234  5678
            displayBot.showString(buf_tmp);
        }


#endif
        
        //TODO: blink disp2 when set button pressed
        //TODO: blink disp2 when preset button pressed (exept manual mode)
        //TODO: write "MAN CTL" to disp2 when in manual mode
        //TODO: display or blink "REACHED" when reached state and start pressed

    }

}
