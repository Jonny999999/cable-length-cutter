#pragma once
extern "C" {
#include "driver/adc.h"
}
#include "gpio_evaluateSwitch.hpp"
#include "buzzer.hpp"
#include "switchesAnalog.hpp"


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
//==== define analog input pins ====
//==================================
#define GPIO_POTI GPIO_NUM_36
#define ADC_CHANNEL_POTI ADC1_CHANNEL_0
#define GPIO_4SW_TO_ANALOG GPIO_NUM_39
#define ADC_CHANNEL_4SW_TO_ANALOG ADC1_CHANNEL_3 //gpio 39
//ADC1_CHANNEL_0 gpio36
//ADC1_CHANNEL_6 gpio_34
//ADC1_CHANNEL_3 gpio_39


//=====================================
//==== assign switches to objects =====
//=====================================
//see config.cpp for available evaluated switch objects
#define SW_START sw_gpio_26
#define SW_RESET sw_gpio_25
#define SW_CUTTER_POS sw_gpio_14
#define SW_SET     sw_gpio_analog_0
#define SW_PRESET1 sw_gpio_analog_1
#define SW_PRESET2 sw_gpio_analog_2
#define SW_PRESET3 sw_gpio_analog_3

//unused but already available evaluated inputs
//#define ? sw_gpio_33
//#define ? sw_gpio_32
//#define ? sw_gpio_34




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
//--- switches on digital gpio pins ---
//extern gpio_evaluatedSwitch sw_gpio_39;
extern gpio_evaluatedSwitch sw_gpio_34;
extern gpio_evaluatedSwitch sw_gpio_32;
extern gpio_evaluatedSwitch sw_gpio_33;
extern gpio_evaluatedSwitch sw_gpio_25;
extern gpio_evaluatedSwitch sw_gpio_26;
extern gpio_evaluatedSwitch sw_gpio_14;

//--- switches connected to 4-sw-to-analog stripboard ---
extern gpio_evaluatedSwitch sw_gpio_analog_0;
extern gpio_evaluatedSwitch sw_gpio_analog_1;
extern gpio_evaluatedSwitch sw_gpio_analog_2;
extern gpio_evaluatedSwitch sw_gpio_analog_3;



//create global buzzer object
extern buzzer_t buzzer;
