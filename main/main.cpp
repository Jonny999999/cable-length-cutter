extern "C"
{
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_idf_version.h>
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/adc.h"

}

#include "config.h"
#include "global.hpp"
#include "control.hpp"
#include "buzzer.hpp"
#include "switchesAnalog.hpp"
#include "guide-stepper.hpp"
#include "encoder.hpp"
#include "shutdown.hpp"

#include "stepper.hpp"


//=================================
//=========== functions ===========
//=================================

//------------------------
//--- configure output ---
//------------------------
//configure a gpio pin as output
void gpio_configure_output(gpio_num_t gpio_pin){
    gpio_pad_select_gpio(gpio_pin);
    gpio_set_direction(gpio_pin, GPIO_MODE_OUTPUT);
}


//--------------------
//---- init gpios ----
//--------------------
void init_gpios(){
    //--- outputs ---
    //4x stepper mosfets
    gpio_configure_output(GPIO_VFD_FWD);
    gpio_configure_output(GPIO_VFD_D0);
    gpio_configure_output(GPIO_VFD_D1);
    gpio_configure_output(GPIO_VFD_REV);
    //gpio_configure_output(GPIO_VFD_D2); only used with 7.5kw vfd
    //2x power mosfets
    gpio_configure_output(GPIO_MOS1); //mos1
    gpio_configure_output(GPIO_LAMP); //llamp (mos2)
    //onboard relay and buzzer
    gpio_configure_output(GPIO_RELAY);
    gpio_configure_output(GPIO_BUZZER);
    //5v regulator
    gpio_configure_output(GPIO_NUM_17);

    //--- inputs ---
    //initialize and configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12); //=> max resolution 4096
    adc1_config_channel_atten(ADC_CHANNEL_POTI, ADC_ATTEN_DB_11); //max voltage

    adc1_config_channel_atten(ADC_CHANNEL_SUPPLY_VOLTAGE, ADC_ATTEN_DB_11); //max voltage
}



//======================================
//============ buzzer task =============
//======================================
void task_buzzer( void * pvParameters ){
    ESP_LOGI("task_buzzer", "Start of buzzer task...");
        //run function that waits for a beep events to arrive in the queue
        //and processes them
        buzzer.processQueue();
}



//======================================
//=========== main function ============
//======================================
extern "C" void app_main()
{
    //init outputs and adc
    init_gpios();

    //enable 5V volage regulator (needed for display)
    gpio_set_level(GPIO_NUM_17, 1);

    //init encoder (global)
    encoder_queue = encoder_init();
    
    //define loglevel
    esp_log_level_set("*", ESP_LOG_INFO); //default loglevel
    esp_log_level_set("buzzer", ESP_LOG_ERROR);
    esp_log_level_set("switches-analog", ESP_LOG_WARN);
    esp_log_level_set("control", ESP_LOG_INFO);
    esp_log_level_set("stepper-driver", ESP_LOG_WARN);
    esp_log_level_set("stepper-ctrl", ESP_LOG_WARN);
    esp_log_level_set("Dendostepper", ESP_LOG_WARN); //stepper lib
    esp_log_level_set("calc", ESP_LOG_WARN); //stepper lib
    esp_log_level_set("lowVoltage", ESP_LOG_INFO);

#ifdef STEPPER_TEST
    //create task for testing the stepper motor
    xTaskCreate(task_stepper_test, "task_stepper_test", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
    //xTaskCreate(task_stepper_debug, "task_stepper_test", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
#else
    //create task for detecting power-off
    xTaskCreate(&task_shutDownDetection, "task_shutdownDet", 2048, NULL, 2, NULL);
    // wait for nvs to be initialized in shutDownDetection task
    vTaskDelay(50 / portTICK_PERIOD_MS);

    //create task for controlling the machine
    xTaskCreate(task_control, "task_control", configMINIMAL_STACK_SIZE * 3, NULL, 4, NULL);

    //create task for controlling the stepper motor (linear axis that guids the cable)
    xTaskCreate(task_stepper_ctl, "task_stepper_ctl", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
#endif

    //create task for handling the buzzer
    xTaskCreate(&task_buzzer, "task_buzzer", 2048, NULL, 2, NULL);

    //beep at startup
    buzzer.beep(3, 70, 50);
}
