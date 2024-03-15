#pragma once

//task that initializes and controls the stepper motor
//current functionality: 
//  - automatically auto-homes
//  - moves left and right repeatedly
//  - updates speed from potentiometer each cycle
void task_stepper_test(void *pvParameter);

//task that initializes and controls the stepper motor
//  - moves stepper according to encoder movement
void task_stepper_ctl(void *pvParameter);


//tell stepper-control task to move cable guide to zero position
void guide_moveToZero();


// return local variable posNow that stores the current position of cable guide axis in steps
// needed by shutdown to store last axis position in nvs
int guide_getAxisPosSteps();

// set custom winding width (axis position the guide returns in mm)
void guide_setWindingWidth(uint8_t maxPosMm);

// get currently configured winding width (axis position the guide returns in mm)
uint8_t guide_getWindingWidth();