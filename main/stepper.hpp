#pragma once

//init stepper pins and timer
void stepper_init();
void stepper_setTargetSteps(int steps);

//task that periodically logs variables for debugging stepper driver
void task_stepper_debug(void *pvParameter);

//control stepper without timer (software)
void task_stepperCtrlSw(void *pvParameter);
void stepperSw_setTargetSteps(uint64_t target);
