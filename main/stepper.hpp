#pragma once

//init stepper pins and timer
void stepper_init();
//set absolute target position in steps
void stepper_setTargetPosSteps(uint64_t steps);
//set absolute target position in millimeters
void stepper_setTargetPosMm(uint32_t posMm);
//set target speed in millimeters per second
void stepper_setSpeed(uint32_t speedMmPerS);

//task that periodically logs variables for debugging stepper driver
void task_stepper_debug(void *pvParameter);
