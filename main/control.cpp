#include "control.hpp"


void task_control(void *pvParameter)
{
    while(1){
       vTaskDelay(500 / portTICK_PERIOD_MS);
    }

}
