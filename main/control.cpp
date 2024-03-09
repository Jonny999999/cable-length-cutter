#include "control.hpp"
#include "encoder.hpp"
#include "guide-stepper.hpp"


//--------------------
//---- variables -----
//--------------------
static const char *TAG = "control"; //tag for logging

const char* systemStateStr[7] = {"COUNTING", "WINDING_START", "WINDING", "TARGET_REACHED", "AUTO_CUT_WAITING", "CUTTING", "MANUAL"};

systemState_t controlState = systemState_t::COUNTING;
static uint32_t timestamp_lastStateChange = 0;

static char buf_disp1[10];// 8 digits + decimal point + \0
static char buf_disp2[10];// 8 digits + decimal point + \0
static char buf_tmp[15];

static int lengthNow = 0; //length measured in mm
static int lengthTarget = 5000; //default target length in mm
static int lengthRemaining = 0; //(target - now) length needed for reaching the target
static int potiRead = 0; //voltage read from adc
static uint32_t timestamp_motorStarted = 0; //timestamp winding started
                                     
//automatic cut
static int cut_msRemaining = 0;
static uint32_t timestamp_cut_lastBeep = 0;
static uint32_t autoCut_delayMs = 2500; //TODO add this to config
static bool autoCutEnabled = false; //store state of toggle switch (no hotswitch)



//--------------------
//---- functions -----
//--------------------

//========================
//===== change State =====
//========================
//function for changing the controlState with log output
void changeState (systemState_t stateNew) {
    //only proceed when state actually changed
    if (controlState == stateNew) {
        return; //already at target state -> nothing to do
    }
    //log change
    ESP_LOGW(TAG, "changed state from %s to %s", systemStateStr[(int)controlState], systemStateStr[(int)stateNew]);
    //change state
    controlState = stateNew;
    //update timestamp
    timestamp_lastStateChange = esp_log_timestamp();
}



//=================================
//===== handle Stop Condition =====
//=================================
//function that checks whether start button is released or target is reached (used in multiple states)
//returns true when stopped, false when no action
bool handleStopCondition(handledDisplay * displayTop, handledDisplay * displayBot){
    //--- stop conditions ---
    //stop conditions that are checked in any mode
    //target reached
    if (lengthRemaining <= 0 ) {
        changeState(systemState_t::TARGET_REACHED);
        vfd_setState(false);
        displayTop->blink(1, 0, 1000, "  S0LL  ");
        displayBot->blink(1, 0, 1000, "ERREICHT");
        buzzer.beep(2, 100, 100);
        return true;
    }
    //start button released
    else if (SW_START.state == false) {
        changeState(systemState_t::COUNTING);
        vfd_setState(false);
        displayTop->blink(2, 900, 1000, "- STOP -");
        displayBot->blink(2, 900, 1000, " TASTER ");
        buzzer.beep(3, 200, 100);
        return true;
    } else {
        return false;
    }
}



//===================================
//===== set dynamic speed level =====
//===================================
//function that sets the vfd speed level dynamically depending on the remaining distance
//closer to target -> slower
void setDynSpeedLvl(uint8_t lvlMax = 3){
    uint8_t lvl;
    //define speed level according to difference
    if (lengthRemaining < 40) {
        lvl = 0;
    } else if (lengthRemaining < 300) {
        lvl = 1;
    } else if (lengthRemaining < 700) {
        lvl = 2;
    } else { //more than last step remaining
        lvl = 3;
    }
    //limit to max lvl
    if (lvl > lvlMax) {
        lvl = lvlMax;
    }
    //update vfd speed level
    vfd_setSpeedLevel(lvl);
}



           
//========================
//===== control task =====
//========================
//task that controls the entire machine
void task_control(void *pvParameter)
{

    //-- initialize display --
    max7219_t two7SegDisplays = display_init();
    //create two separate custom handled display instances
    handledDisplay displayTop(two7SegDisplays, 0);
    handledDisplay displayBot(two7SegDisplays, 8);

    //-- display welcome msg --
    //display welcome message on two 7 segment displays
    //currently show name and date and scrolling 'hello'
    display_ShowWelcomeMsg(two7SegDisplays);


    //===== loop =====
    while(1){
        vTaskDelay(10 / portTICK_PERIOD_MS);

        //------ handle switches ------
        //run handle functions for all switches
        SW_START.handle();
        SW_RESET.handle();
        SW_SET.handle();
        SW_PRESET1.handle();
        SW_PRESET2.handle();
        SW_PRESET3.handle();
        SW_CUT.handle();
        SW_AUTO_CUT.handle();


        //------ handle cutter ------
        //TODO: separate task for cutter?
        cutter_handle();


        //------ rotary encoder ------
        //get current length since last reset
        lengthNow = encoder_getLenMm();

        
        //--------- buttons ---------
        //#### RESET switch ####
        if (SW_RESET.risingEdge) {
            //dont reset when press used for stopping pending auto-cut
            if (controlState != systemState_t::AUTO_CUT_WAITING) {
                guide_moveToZero(); //move axis guiding the cable to start position
                encoder_reset(); //reset length measurement
                lengthNow = 0;
                buzzer.beep(1, 700, 100);
                displayTop.blink(2, 100, 100, "1ST     ");
                //TODO: stop cutter with reset switch?
                //cutter_stop();
            }
        }

        //### CUT switch ####
        //start cut cycle immediately
        if (SW_CUT.risingEdge) {
            //stop cutter if already running
            if (cutter_isRunning()) {
                cutter_stop();
                buzzer.beep(1, 600, 0);
            }
            else if (controlState == systemState_t::AUTO_CUT_WAITING) {
                //do nothing when press used for stopping pending auto-cut
            }
            //start cutter when motor not active
            else if (controlState != systemState_t::WINDING_START //TODO use vfd state here?
                    && controlState != systemState_t::WINDING) {
                cutter_start();
                buzzer.beep(1, 70, 50);
            }
            //error cant cut while motor is on
            else {
                buzzer.beep(6, 100, 50);
            }
        }
       
        //#### AUTO_CUT toggle sw ####
        //beep at change
        if (SW_AUTO_CUT.risingEdge) {
            buzzer.beep(2, 100, 50);
        } else if (SW_AUTO_CUT.fallingEdge) {
            buzzer.beep(1, 400, 50);
        }
        //update enabled state
        if (SW_AUTO_CUT.state) {
            //enable autocut when not in target_reached state
            //(prevent immediate/unexpected cut)
            if (controlState != systemState_t::TARGET_REACHED) {
                autoCutEnabled = true;
            }
        } else {
            //disable anytime (also stops countdown to auto cut)
            autoCutEnabled = false;
        }

        //#### manual mode ####
        //switch to manual motor control (2 buttons + poti)
        if ( SW_PRESET2.state && (SW_PRESET1.state || SW_PRESET3.state) && controlState != systemState_t::MANUAL ) {
            //enable manual control
            changeState(systemState_t::MANUAL);
            buzzer.beep(3, 100, 60);
        }

        //##### custom target length using poti #####
        //set target length to poti position when SET switch is pressed
        if (SW_SET.state == true) {
            //read adc
            potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095
            //scale to target length range
            int lengthTargetNew = (float)potiRead / 4095 * 50000;
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


        //##### target length preset buttons #####
        if (controlState != systemState_t::MANUAL) { //dont apply preset length while controlling motor with preset buttons
            if (SW_PRESET1.risingEdge) {
                lengthTarget = 5000;
                buzzer.beep(lengthTarget/1000, 25, 30);
                displayBot.blink(2, 100, 100, "S0LL    ");
            }
            else if (SW_PRESET2.risingEdge) {
                lengthTarget = 10000;
                buzzer.beep(lengthTarget/1000, 25, 30);
                displayBot.blink(2, 100, 100, "S0LL    ");
            }
            else if (SW_PRESET3.risingEdge) {
                lengthTarget = 15000;
                buzzer.beep(lengthTarget/1000, 25, 30);
                displayBot.blink(2, 100, 100, "S0LL    ");
            }
        }


        //---------------------------
        //--------- control ---------
        //---------------------------
        //statemachine handling the sequential winding process

        //calculate current length difference
        lengthRemaining = lengthTarget - lengthNow + TARGET_LENGTH_OFFSET;

        //--- statemachine ---
        switch (controlState) {
            case systemState_t::COUNTING: //no motor action, just show current length on display
                vfd_setState(false);
                //TODO check stop condition before starting - prevents motor from starting 2 cycles when already at target
                //--- start winding to length ---
                if (SW_START.risingEdge) {
                    changeState(systemState_t::WINDING_START);
                    vfd_setSpeedLevel(1); //start at low speed 
                    vfd_setState(true); //start motor
                    timestamp_motorStarted = esp_log_timestamp(); //save time started
                    buzzer.beep(1, 100, 0);
                } 
                break;

            case systemState_t::WINDING_START: //wind slow for certain time
                //set vfd speed depending on remaining distance 
                setDynSpeedLvl(1); //limit to speed lvl 1 (force slow start)
                //switch to WINDING state (full speed) when 3s have passed
                if (esp_log_timestamp() - timestamp_motorStarted > 3000) {
                    changeState(systemState_t::WINDING);
                }
                handleStopCondition(&displayTop, &displayBot); //stops if button released or target reached
                //TODO: cancel when there was no cable movement during start time?
                break;

            case systemState_t::WINDING: //wind fast, slow down when close
                //set vfd speed depending on remaining distance 
                setDynSpeedLvl(); //set motor speed, slow down when close to target
                handleStopCondition(&displayTop, &displayBot); //stops if button released or target reached
                //TODO: cancel when there is no cable movement anymore e.g. empty / timeout?
                break;

            case systemState_t::TARGET_REACHED: //prevent further motor rotation and start auto-cut
                vfd_setState(false);
                //switch to counting state when no longer at or above target length
                if ( lengthNow < lengthTarget - TARGET_REACHED_TOLERANCE ) {
                    changeState(systemState_t::COUNTING);
                }
                //initiate countdown to auto-cut if enabled
                else if ( (autoCutEnabled)
                        && (esp_log_timestamp() - timestamp_lastStateChange > 300) ) { //wait for dislay msg "reached" to finish
                    changeState(systemState_t::AUTO_CUT_WAITING);
                }
                //show msg when trying to start, but target is already reached (-> reset button has to be pressed)
                if (SW_START.risingEdge) {
                    buzzer.beep(2, 50, 30);
                    displayTop.blink(2, 600, 500, "  S0LL  ");
                    displayBot.blink(2, 600, 500, "ERREICHT");
                }
                break;

            case systemState_t::AUTO_CUT_WAITING: //handle delayed start of cut
                cut_msRemaining = autoCut_delayMs - (esp_log_timestamp() - timestamp_lastStateChange);
                //- countdown stop conditions -
                //stop with any button
                if (!autoCutEnabled
                        || SW_RESET.state || SW_CUT.state 
                        || SW_SET.state || SW_PRESET1.state
                        || SW_PRESET2.state || SW_PRESET3.state) {//TODO: also stop when target not reached anymore?
                    changeState(systemState_t::COUNTING);
                    buzzer.beep(5, 100, 50);
                }
                //- trigger cut if delay passed -
                else if (cut_msRemaining <= 0) {
                    cutter_start();
                    changeState(systemState_t::CUTTING);
                }
                //- beep countdown -
                //time passed since last beep  >  time remaining / 6
                else if ( (esp_log_timestamp() - timestamp_cut_lastBeep)  > (cut_msRemaining / 6)
                        && (esp_log_timestamp() - timestamp_cut_lastBeep) > 50 ) { //dont trigger beeps faster than beep time
                    buzzer.beep(1, 50, 0);
                    timestamp_cut_lastBeep = esp_log_timestamp();
                }
                break;

            case systemState_t::CUTTING: //prevent any action while cutter is active
                //exit when finished cutting
                if (cutter_isRunning() == false) {
                    //TODO stop if start buttons released?
                    changeState(systemState_t::COUNTING);
                    //TODO reset automatically or wait for manual reset?
                    guide_moveToZero(); //move axis guiding the cable to start position
                    encoder_reset(); //reset length measurement
                    lengthNow = 0;
                    buzzer.beep(1, 700, 100);
                }
                break;

            case systemState_t::MANUAL: //manually control motor via preset buttons + poti
                //read poti value
                potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095
                //scale poti to speed levels 0-3
                uint8_t level = round( (float)potiRead / 4095 * 3 );
                //exit manual mode if preset2 released
                if ( SW_PRESET2.state == false ) {
                    changeState(systemState_t::COUNTING);
                    buzzer.beep(1, 1000, 100);
                }
                //P2 + P1 -> turn left
                else if ( SW_PRESET1.state && !SW_PRESET3.state ) {
                    vfd_setSpeedLevel(level);
                    vfd_setState(true, REV);
                    sprintf(buf_disp2, "[--%02i   ", level);
                    //                  123 45 678
                }
                //P2 + P3 -> turn right
                else if ( SW_PRESET3.state && !SW_PRESET1.state ) {
                    vfd_setSpeedLevel(level);
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
        sprintf(buf_disp1, "EN %05d", encoder_getSteps); //count
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
        //indicate upcoming cut when pending
        if (controlState == systemState_t::AUTO_CUT_WAITING) {
            displayTop.blinkStrings(" CUT 1N ", "        ", 70, 30);
        }
        //otherwise show current position
        else {
            sprintf(buf_tmp, "1ST %5.4f", (float)lengthNow/1000);
            //                123456789
            //limit length to 8 digits + decimal point (drop decimal places when it does not fit)
            sprintf(buf_disp1, "%.9s", buf_tmp);
            displayTop.showString(buf_disp1);
        }


        //--------------------------
        //-------- display2 --------
        //--------------------------
        //run handle function
        displayBot.handle();
        //setting target length: blink target length
        if (SW_SET.state == true) {
            sprintf(buf_tmp, "S0LL%5.3f", (float)lengthTarget/1000);
            displayBot.blinkStrings(buf_tmp, "S0LL    ", 300, 100);
        }
        //manual state: blink "manual"
        else if (controlState == systemState_t::MANUAL) {
            displayBot.blinkStrings(" MANUAL ", buf_disp2, 400, 800);
        }
        //notify that cutter is active
        else if (cutter_isRunning()) {
            displayBot.blinkStrings("CUTTING]", "CUTTING[", 100, 100);
        }
        //show ms countdown to cut when pending
        else if (controlState == systemState_t::AUTO_CUT_WAITING) {
            sprintf(buf_disp2, "  %04d  ", cut_msRemaining);
            //displayBot.showString(buf_disp2); //TODO:blink "erreicht" overrides this. for now using blink as workaround
            displayBot.blinkStrings(buf_disp2, buf_disp2, 100, 100);
        }
        //otherwise show target length
        else {
            //sprintf(buf_disp2, "%06.1f cm", (float)lengthTarget/10); //cm
            sprintf(buf_tmp, "S0LL%5.3f", (float)lengthTarget/1000); //m
            //                1234  5678
            displayBot.showString(buf_tmp);
        }


        //----------------------------
        //------- control lamp -------
        //----------------------------
        //basic functionality of lamp:
        //turn on when not idling
        //TODO: add switch-case for different sates
        //e.g. blink with different frequencies in different states
        if (controlState != systemState_t::COUNTING
                && controlState != systemState_t::TARGET_REACHED) {
            gpio_set_level(GPIO_LAMP, 1);
        }
        else {
            gpio_set_level(GPIO_LAMP, 0);
        }



#endif
        
    } //end while(1)

} //end task_control
