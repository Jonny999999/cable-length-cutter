#pragma once


//enum describing the state of the system
enum class systemState_t {COUNTING, WINDING_START, WINDING, TARGET_REACHED, AUTO_CUT_WAITING, CUTTING, MANUAL};

//array with enum as strings for logging states
extern const char* systemStateStr[7];


//task that controls the entire machine (has to be created as task in main function)
void task_control(void *pvParameter);
