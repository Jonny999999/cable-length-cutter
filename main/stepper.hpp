#pragma once

//init stepper pins and timer
void stepper_init();
void stepper_setTargetSteps(int steps);

//control stepper without timer (software)
void task_stepperCtrlSw(void *pvParameter);
void stepperSw_setTargetSteps(uint64_t target);
