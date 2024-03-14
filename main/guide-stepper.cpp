extern "C"
{
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "freertos/queue.h"
}

#include "stepper.hpp"
#include "config.h"
#include "global.hpp"
#include "guide-stepper.hpp"
#include "encoder.hpp"
#include "shutdown.hpp"


//macro to get smaller value out of two
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

//---------------------
//--- configuration ---
//---------------------
//also see config.h
//for pin definition

#define STEPPER_TEST_TRAVEL 65     // mm

#define MIN_MM 0 //TODO add feature so guide stays at zero for some steps (negative MIN_MM?), currently seems appropriate for even winding
#define MAX_MM 95 //actual reel is 110, but currently guide turned out to stay at max position for too long
#define POS_MAX_STEPS MAX_MM * STEPPER_STEPS_PER_MM
#define POS_MIN_STEPS MIN_MM * STEPPER_STEPS_PER_MM

//tolerance added to last stored position at previous shutdown.
//When calibrating at startup the stepper moves for that sum to get track of zero position (ensure crashes into hardware limit for at least some time)
#define AUTO_HOME_TRAVEL_ADD_TO_LAST_POS_MM 20
#define MAX_TOTAL_AXIS_TRAVEL_MM 103 //max possible travel distance, needed for auto-home

// speeds for testing with potentiometer (test task only)
#define SPEED_MIN 2.0   // mm/s
#define SPEED_MAX 70.0  // mm/s
//note: actual speed is currently defined in config.h with STEPPER_SPEED_DEFAULT

#define LAYER_THICKNESS_MM 5 //height of one cable layer on reel -> increase in radius
#define D_CABLE 6
#define D_REEL 160
#define PI 3.14159


//simulate encoder with reset button to test stepper ctl task
//note STEPPER_TEST has to be defined as well
//#define STEPPER_SIMULATE_ENCODER

//----------------------
//----- variables ------
//----------------------
typedef enum axisDirection_t {AXIS_MOVING_LEFT = 0, AXIS_MOVING_RIGHT} axisDirection_t;

static const char *TAG = "stepper-ctrl"; //tag for logging

static axisDirection_t currentAxisDirection = AXIS_MOVING_RIGHT;
static uint32_t posNow = 0;

static int layerCount = 0;

// store optimal distance traveled when homing (determined by last pos in nvs)
static int homeTravelDistance = MAX_TOTAL_AXIS_TRAVEL_MM;
// track if already homed (either homes at startup or at first reset when there is still cable on the reel)
static bool axisIsHomed = false;

// queue for sending commands to task handling guide movement
static QueueHandle_t queue_commandsGuideTask;


//----------------------
//----- functions ------
//----------------------

//=============================
//=== guide_getAxisPosSteps ===
//=============================
// return local variable posNow
// needed at shutdown detection to store last axis position in nvs
int guide_getAxisPosSteps(){
    return posNow;
}


//==========================
//==== guide_moveToZero ====
//==========================
//tell stepper-control task to move cable guide to zero position
void guide_moveToZero(){
    //home axis first if not done already at startup
    if (!axisIsHomed){
        ESP_LOGW(TAG, "moveToZero: Axis not homed yet due to high previous length -> homing now");
        stepper_home(homeTravelDistance);
        axisIsHomed = true;
    }
    bool valueToSend = true; // or false - currently not relevant
    xQueueSend(queue_commandsGuideTask, &valueToSend, portMAX_DELAY);
    ESP_LOGI(TAG, "sending command to stepper_ctl task via queue");
}


//---------------------
//---- travelSteps ----
//---------------------
//move axis certain Steps (relative) between left and right or reverse when negative
void travelSteps(int stepsTarget){
	//TODO simplify this function, one simple calculation of new position?
	//with new custom driver no need to detect direction change

    int stepsToGo, remaining;

    stepsToGo = abs(stepsTarget);

    // invert direction in reverse mode (cable gets spooled off reel)
    if (stepsTarget < 0) {
    currentAxisDirection = (currentAxisDirection == AXIS_MOVING_LEFT) ? AXIS_MOVING_RIGHT : AXIS_MOVING_LEFT; //toggle between RIGHT<->Left
    }

    while (stepsToGo != 0){
        //--- currently moving right ---
        if (currentAxisDirection == AXIS_MOVING_RIGHT){               //currently moving right
            remaining = POS_MAX_STEPS - posNow;     //calc remaining distance fom current position to limit
            if (stepsToGo > remaining){             //new distance will exceed limit
                stepper_setTargetPosSteps(POS_MAX_STEPS);        //move to limit
				stepper_waitForStop(1000);
                posNow = POS_MAX_STEPS;
                currentAxisDirection = AXIS_MOVING_LEFT;            //change current direction for next iteration
                //increment/decrement layer count depending on current cable direction
                layerCount += (stepsTarget > 0) - (stepsTarget < 0);
                if (layerCount < 0) layerCount = 0; //negative layers are not possible
                stepsToGo = stepsToGo - remaining;  //decrease target length by already traveled distance
                ESP_LOGI(TAG, " --- moved to max -> change direction (L) --- \n ");
            }
            else {                                  //target distance does not reach the limit
                stepper_setTargetPosSteps(posNow + stepsToGo);   //move by (remaining) distance to reach target length
                ESP_LOGD(TAG, "moving to %d\n", posNow+stepsToGo);
                posNow += stepsToGo;
                stepsToGo = 0;                      //finished, reset target length (could as well exit loop/break)
            }
        }

        //--- currently moving left ---
        else if (currentAxisDirection == AXIS_MOVING_LEFT){
            remaining = posNow - POS_MIN_STEPS;
            if (stepsToGo > remaining){
                stepper_setTargetPosSteps(POS_MIN_STEPS);
				stepper_waitForStop(1000);
                posNow = POS_MIN_STEPS;
                currentAxisDirection = AXIS_MOVING_RIGHT; //switch direction
                //increment/decrement layer count depending on current cable direction
                layerCount += (stepsTarget > 0) - (stepsTarget < 0);
                if (layerCount < 0) layerCount = 0; //negative layers are not possible
                stepsToGo = stepsToGo - remaining;
                ESP_LOGI(TAG, " --- moved to min -> change direction (R) --- \n ");
            }
            else {
                stepper_setTargetPosSteps(posNow - stepsToGo);           //when moving left the coordinate has to be decreased
                ESP_LOGD(TAG, "moving to %d\n", posNow - stepsToGo);
                posNow -= stepsToGo;
                stepsToGo = 0;
            }
        }
    }

    // undo inversion of currentAxisDirection after reverse mode is finished
    if (stepsTarget < 0) {
    currentAxisDirection = (currentAxisDirection == AXIS_MOVING_LEFT) ? AXIS_MOVING_RIGHT : AXIS_MOVING_LEFT; //toggle between RIGHT<->Left
    }

    return;
}


//------------------------
//---- lengthToLayers ----
//------------------------
// calculate cable layers on reel from a certain cable length
// TODO: currently assumes windingWidth is constant (MAX_MM)
uint16_t lengthToLayers(float lengthMm){
    float windingWidth = MAX_MM;
    uint16_t layerCount = 0;
    //cable length required for one layer
    float lenNextLayer = (PI * D_REEL) * (windingWidth / D_CABLE);
    uint32_t lengthRemaining = lengthMm;

    while (lengthRemaining >= lenNextLayer){
        float lenThisLayer = lenNextLayer; 
        lengthRemaining -= lenThisLayer;
        layerCount++;

        float currentDiameter = D_REEL + LAYER_THICKNESS_MM * 2 * layerCount;
        lenNextLayer = (PI * currentDiameter) * windingWidth/D_CABLE;
    }

    ESP_LOGI(TAG, "lengthToLayers: calculated %d layers from length=%.1fm, windWidth=%.1fmm, D-cable=%dmm, D-start=%dmm", layerCount, lengthMm/1000, windingWidth, D_CABLE, D_REEL);
    return layerCount;
}


//------------------
//---- travelMm ----
//------------------
//move axis certain Mm (relative) between left and right or reverse when negative
void travelMm(int length){
    travelSteps(length * STEPPER_STEPS_PER_MM);
}


//----------------------
//---- init_stepper ----
//----------------------
//initialize/configure stepper instance
void init_stepper() {
	//TODO unnecessary wrapper?
    ESP_LOGW(TAG, "initializing stepper...");
	stepper_init();
    // create queue for sending commands to task handling guide movement
    // currently length 1 and only one command possible, thus bool
    queue_commandsGuideTask = xQueueCreate(1, sizeof(bool));
}


//--------------------------
//--- updateSpeedFromAdc ---
//--------------------------
//function that updates speed value using ADC input and configured MIN/MAX - used for testing only
void updateSpeedFromAdc() {
    int potiRead = gpio_readAdc(ADC_CHANNEL_POTI); //0-4095 GPIO34
    double poti = potiRead/4095.0;
    int speed = poti*(SPEED_MAX-SPEED_MIN) + SPEED_MIN;
	stepper_setSpeed(speed);
    ESP_LOGW(TAG, "poti: %d (%.2lf%%), set speed to: %d", potiRead, poti*100, speed);
}



//============================
//==== TASK stepper_test =====
//============================
//test axis without using encoder input
#ifndef STEPPER_SIMULATE_ENCODER
void task_stepper_test(void *pvParameter)
{
	stepper_init();
	while(1){
		vTaskDelay(20 / portTICK_PERIOD_MS);

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

#ifdef ONE_BUTTON_TEST //test with "reset-button" only
        static int state = 0;
        //cycle through test commands with one button
		if (SW_RESET.risingEdge) {
			switch (state){
				case 0:
					stepper_setTargetPosMm(50);
					//stepper_setTargetPosSteps(1000);
					state++;
					break;
				case 1:
					stepper_setTargetPosMm(80);
					//stepper_setTargetPosSteps(100);
					state++;
					break;
				case 2:
					stepper_setTargetPosMm(20);
					//stepper_setTargetPosSteps(100);
					state++;
					break;
				case 3:
					stepper_setTargetPosMm(60);
					//stepper_setTargetPosSteps(2000);
					state = 0;
					break;
			}
		}
#else //test with all buttons
		if (SW_RESET.risingEdge) {
			buzzer.beep(1, 500, 100);
			stepper_setTargetPosMm(0);
		}
		if (SW_PRESET1.risingEdge) {
			buzzer.beep(1, 200, 100);
			stepper_setTargetPosMm(50);
		}
		if (SW_PRESET2.risingEdge) {
			buzzer.beep(2, 200, 100);
			stepper_setTargetPosMm(75);
		}
		if (SW_PRESET3.risingEdge) {
			buzzer.beep(3, 200, 100);
			stepper_setTargetPosMm(100);
		}
#endif
	}
}
#endif //end SIMULATE_ENCODER



//============================
//===== TASK stepper_ctl =====
//============================
//task controlling the linear axis guiding the cable according to wire length spooled
#ifdef STEPPER_SIMULATE_ENCODER
void task_stepper_test(void *pvParameter)
#else
void task_stepper_ctl(void *pvParameter)
#endif
{
    //-- variables --
    static int encStepsPrev = 0; //measured encoder steps at last run
    static double travelStepsPartial = 0; //store resulted remaining partial steps last run

    //temporary variables for calculating required steps, or logging
    int encStepsNow = 0; //get curretly measured steps of encoder
    int encStepsDelta = 0; //encoder steps changed since last iteration

    double cableLen = 0;
    double travelStepsExact = 0; //steps axis has to travel
    int travelStepsFull = 0;
    double travelMm = 0;
    double turns = 0;
    float currentDiameter;



    //-- initialize stepper --
    init_stepper();

    //-- consider axis-position at last shutdown --
    // use last known position stored at last shutdown to reduce time crashing into hardware limit
    int posLastShutdown = nvsReadLastAxisPosSteps();
    if (posLastShutdown >= 0)
    { // would be -1 when failed
        ESP_LOGW(TAG, "auto-home: considerting pos last shutdown %dmm + tolerance %dmm",
        posLastShutdown / STEPPER_STEPS_PER_MM, 
        AUTO_HOME_TRAVEL_ADD_TO_LAST_POS_MM);
        // home considering last position and offset, but limit to max distance possible
         homeTravelDistance = MIN((posLastShutdown/STEPPER_STEPS_PER_MM + AUTO_HOME_TRAVEL_ADD_TO_LAST_POS_MM), MAX_TOTAL_AXIS_TRAVEL_MM);
    }
    else { // default to max travel when read from nvs failed
        stepper_home(MAX_TOTAL_AXIS_TRAVEL_MM);
        homeTravelDistance = MAX_TOTAL_AXIS_TRAVEL_MM; //note: is default value anyways
    }

    //-- consider measured length at last shutdown --
    //read length at last power-off
    int encoderPosLastShutdown = nvsReadLastEncoderSteps();
    // when significant length stored: calculate layer count, dont home stepper yet (only at next RESET button press)
    if (encoderPosLastShutdown > 2*ENCODER_STEPS_PER_METER){
        layerCount = lengthToLayers((float)encoderPosLastShutdown/ENCODER_STEPS_PER_METER * 1000);
        ESP_LOGW(TAG, "last stored length is %.1fm -> set layer count to %d", (float)encoderPosLastShutdown/ENCODER_STEPS_PER_METER, layerCount);
    }
    else { //last stored length is not significant: home stepper
        stepper_home(homeTravelDistance);
        axisIsHomed = true;
    }

    //repeatedly read changes in measured cable length and move axis accordingly
    while(1){
#ifdef STEPPER_SIMULATE_ENCODER
		//TESTING - simulate encoder using switch
		SW_RESET.handle();
		//note 
		if (SW_RESET.risingEdge) encStepsNow += 500;
#else
        //get current length
        encStepsNow = encoder_getSteps();
#endif

        // move to zero and reset if command received via queue
        bool receivedValue;
        if (xQueueReceive(queue_commandsGuideTask, &receivedValue, 0) == pdTRUE)
        {
            // Process the received value
            // TODO support other commands (currently only move to zero possible)
            ESP_LOGW(TAG, "task: move-to-zero command received via queue, starting move, waiting until position reached");
            stepper_setTargetPosMm(0);
            stepper_waitForStop();
            //reset variables -> start tracking cable movement starting from position zero
            // ensure stepsDelta is 0
            encStepsNow = encoder_getSteps();
            encStepsPrev = encStepsNow;
            travelStepsPartial = 0;
            // set locally stored axis position and counted layers to 0 (used for calculating the target axis coordinate and steps)
            posNow = 0;
            layerCount = 0;
            currentAxisDirection = AXIS_MOVING_RIGHT;
            ESP_LOGW(TAG, "at position 0, reset variables, resuming normal cable guiding operation");
        }

        //calculate change
        encStepsDelta = encStepsNow - encStepsPrev;
        //check if reset happend without moving to zero before - resulting in huge diff
        if (encStepsDelta != 0 && encStepsNow == 0){  // this should not happen and causes weird movement
            ESP_LOGE(TAG, "encoder steps changed to 0 (reset) without previous moveToZero() call, resulting in stepsDelta=%d", encStepsDelta);
        }

        //read potentiometer and normalize (0-1) to get a variable for testing
        //float potiModifier = (float) gpio_readAdc(ADC_CHANNEL_POTI) / 4095; //0-4095 -> 0-1
        //ESP_LOGI(TAG, "current poti-modifier = %f", potiModifier);

        //calculate steps to move
        cableLen = (double)encStepsDelta * 1000 / ENCODER_STEPS_PER_METER;
        //effective diameter increases each layer
        currentDiameter = D_REEL + LAYER_THICKNESS_MM * 2 * layerCount;
        turns = cableLen / (PI * currentDiameter);
        travelMm = turns * D_CABLE;
        travelStepsExact = travelMm * STEPPER_STEPS_PER_MM  +  travelStepsPartial; //convert mm to steps and add not moved partial steps
        travelStepsPartial = 0;
        travelStepsFull = (int)travelStepsExact;

        //move axis when ready to move at least 1 full step
        if (abs(travelStepsFull) > 1){
            travelStepsPartial = fmod(travelStepsExact, 1); //save remaining partial steps to be added in the next iteration
            ESP_LOGI(TAG, "dCablelen=%.2lf, dTurns=%.2lf, travelMm=%.3lf, StepsExact: %.3lf, StepsFull=%d, StepsPartial=%.3lf, totalLayerCount=%d, diameter=%.1f", cableLen, turns, travelMm, travelStepsExact, travelStepsFull, travelStepsPartial, layerCount, currentDiameter);
            ESP_LOGD(TAG, "MOVING %d steps", travelStepsFull);
            travelSteps(travelStepsExact);
            encStepsPrev = encStepsNow; //update previous length
        }
        else {
            //TODO use encoder queue to only run this check at encoder event?
            vTaskDelay(5);
        }
		vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}
