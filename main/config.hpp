#pragma once
extern "C" {
#include "driver/adc.h"
}
#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"


//===================================
//===== define output gpio pins =====
//===================================
//4x stepper mosfet outputs for VFD
#define GPIO_VFD_FWD GPIO_NUM_4     //ST4
#define GPIO_VFD_REV GPIO_NUM_16    //ST3
#define GPIO_VFD_D0 GPIO_NUM_2      //ST2
#define GPIO_VFD_D1 GPIO_NUM_15     //ST1
//#define GPIO_VFD_D2 GPIO_NUM_15     //ST1 (D2 only used with 7.5kw vfd)

#define GPIO_MOS2 GPIO_NUM_5        //mos2
#define GPIO_RELAY GPIO_NUM_13
#define GPIO_BUZZER GPIO_NUM_12


//==================================
//===== define input gpio pins =====
//==================================
#define GPIO_SW_START GPIO_NUM_26
#define GPIO_SW_RESET GPIO_NUM_25
#define GPIO_SW_SET GPIO_NUM_33
#define GPIO_SW_PRESET1 GPIO_NUM_32
#define GPIO_SW_PRESET2 GPIO_NUM_34
#define GPIO_SW_PRESET3 GPIO_NUM_39

#define GPIO_CUTTER_POS_SW GPIO_NUM_14
#define GPIO_POTI GPIO_NUM_36
#define ADC_CHANNEL_POTI ADC1_CHANNEL_0


//--------------------------
//----- display config -----
//--------------------------
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 0, 0)
#define HOST    HSPI_HOST
#else
#define HOST    SPI2_HOST
#endif
#define DISPLAY_PIN_NUM_MOSI GPIO_NUM_23
#define DISPLAY_PIN_NUM_CLK GPIO_NUM_22
#define DISPLAY_PIN_NUM_CS GPIO_NUM_27
#define DISPLAY_DELAY 2000

//--------------------------
//----- encoder config -----
//--------------------------
#define ROT_ENC_A_GPIO GPIO_NUM_19
#define ROT_ENC_B_GPIO GPIO_NUM_21
#define ENABLE_HALF_STEPS false  // Set to true to enable tracking of rotary encoder at half step resolution
#define FLIP_DIRECTION    false  // Set to true to reverse the clockwise/counterclockwise sense

//--------------------------
//------ calibration -------
//--------------------------
//use encoder test for calibration and calculate STEPS_PER_METER
//#define ENCODER_TEST //show encoder count instead of converted meters
#define STEPS_PER_METER 2127 //roll-v3-gummi-86.6mm - d=89.8mm
//#define MEASURING_ROLL_DIAMETER 86.6 //roll v3 large
//#define PI 3.14159265358979323846



//============================
//===== global variables =====
//============================
//create global evaluated switch objects
//--- inputs ---
//create objects for switches at bottom screw temerinals
extern gpio_evaluatedSwitch SW_START;
extern gpio_evaluatedSwitch SW_RESET;
extern gpio_evaluatedSwitch SW_SET;
extern gpio_evaluatedSwitch SW_PRESET1;
extern gpio_evaluatedSwitch SW_PRESET2;
extern gpio_evaluatedSwitch SW_PRESET3;

//create global buzzer object
extern buzzer_t buzzer;
