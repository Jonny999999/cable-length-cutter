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
