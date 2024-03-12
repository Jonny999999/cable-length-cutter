extern "C"
{
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_system.h"
#include "esp_log.h"
#include "driver/adc.h"
#include "nvs_flash.h"
#include "nvs.h"
}

#include "config.h"
#include "shutdown.hpp"

#include "guide-stepper.hpp"

#define ADC_LOW_VOLTAGE_THRESHOLD 3200 // adc value where shut down is detected (store certain values before complete power loss)

static const char *TAG = "lowVoltage"; // tag for logging
nvs_handle_t nvsHandle; // access nvs that was opened once, in any function



// store a u32 value in nvs as "lastPosSteps"
void nvsWriteLastAxisPos(uint32_t value)
{
    // update nvs value
    esp_err_t err = nvs_set_u32(nvsHandle, "lastPosSteps", value);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed writing");
    err = nvs_commit(nvsHandle);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "nvs: failed committing updates");
    else
        ESP_LOGI(TAG, "nvs: successfully committed updates");
    ESP_LOGW(TAG, "updated value in nvs to %d", value);
}



// read "lastPosSteps" from nvs, returns -1 if failed
int nvsReadLastAxisPosSteps()
{
    uint32_t valueRead;
    esp_err_t err = nvs_get_u32(nvsHandle, "lastPosSteps", &valueRead);
    switch (err)
    {
    case ESP_OK:
        ESP_LOGW(TAG, "Successfully read value %d from nvs", valueRead);
        return valueRead;
        break;
    case ESP_ERR_NVS_NOT_FOUND:
        ESP_LOGE(TAG, "nvs: the value '%s' is not initialized yet", "lastPosSteps");
        return -1;
        break;
    default:
        ESP_LOGE(TAG, "Error (%s) reading nvs!", esp_err_to_name(err));
        return -1;
    }
}



// task that repeatedly checks supply voltage (12V) and saves certain values to nvs in case of power off detected
// note: with the 2200uF capacitor in the 12V supply and measuring if 12V start do drop here, there is more than enough time to take action until the 3v3 regulator turns off
void task_shutDownDetection(void *pvParameter)
{
    //--- Initialize NVS ---
    ESP_LOGW(TAG, "initializing nvs...");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGE(TAG, "NVS truncated -> deleting flash");
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }

    //--- open nvs-flash ---
    ESP_LOGW(TAG, "opening NVS-handle...");
    // create handle available for all functions in this file
    err = nvs_open("storage", NVS_READWRITE, &nvsHandle);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));

    // read stored value (returns 0 if unitialized/failed)
    //int lastPosSteps = nvsReadLastAxisPosSteps();
    //ESP_LOGW(TAG, "=> read value %d from nvs (stored at last shutdown)", lastPosSteps);

    // repeatedly read ADC and check if below low voltage threshold
    bool voltageBelowThreshold = false;
    while (1) //TODO limit save frequency in case voltage repeadedly varys between threshold for some reason (e.g. offset drift)
    {
        // read adc
        int adc_reading = adc1_get_raw(ADC_CHANNEL_SUPPLY_VOLTAGE);

        // evaulate threshold
        if (adc_reading < ADC_LOW_VOLTAGE_THRESHOLD) // below threshold => POWER SHUTDOWN DETECTED
        {
            // write to nvs and log once at change to below
            if (!voltageBelowThreshold){
                nvsWriteLastAxisPos(guide_getAxisPosSteps());
                ESP_LOGE(TAG, "voltage now below threshold!  now=%d threshold=%d -> wrote last axis-pos to nvs", adc_reading, ADC_LOW_VOLTAGE_THRESHOLD);
                voltageBelowThreshold = true;
            }
        }
        else if (voltageBelowThreshold) // above threshold and previously below
        {
            // log at change to above
            ESP_LOGE(TAG, "voltage above threshold again: %d > %d - issue with power supply, or too threshold too high?", adc_reading, ADC_LOW_VOLTAGE_THRESHOLD);
            voltageBelowThreshold = false;
        }

        // always log for debugging/calibrating
        ESP_LOGD(TAG, "read adc battery voltage: %d", adc_reading);

        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
}