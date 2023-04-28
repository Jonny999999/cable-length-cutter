#pragma once

//init stepper pins and timer
void stepper_init();

//delay until stepper is stopped, optional timeout in ms, 0 = no limit
void stepper_waitForStop(uint32_t timeoutMs = 0);

//run to limit and define zero/start position. (busy until finished)
//Currently simply runs stepper for travelMm and bumps into hardware limit
void stepper_home(uint32_t travelMm = 60);

//set absolute target position in steps
void stepper_setTargetPosSteps(uint64_t steps);

//set absolute target position in millimeters
void stepper_setTargetPosMm(uint32_t posMm);

//set target speed in millimeters per second
void stepper_setSpeed(uint32_t speedMmPerS);



//task that periodically logs variables for debugging stepper driver
void task_stepper_debug(void *pvParameter);
