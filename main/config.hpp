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
#define GPIO_VFD_REV GPIO_NUM_5     //mos2
#define GPIO_VFD_D0 GPIO_NUM_2      //ST2
#define GPIO_VFD_D1 GPIO_NUM_15     //ST1
//#define GPIO_VFD_D2 GPIO_NUM_15     //ST1 (D2 only used with 7.5kw vfd)

#define GPIO_MOS1 GPIO_NUM_18       //mos1 (free) 2022.02.28: pin used for stepper
#define GPIO_LAMP GPIO_NUM_0        //mos2 (5) 2022.02.28: lamp disabled, pin used for stepper
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
#define SW_CUT sw_gpio_33
#define SW_AUTO_CUT sw_gpio_32

//unused but already available evaluated inputs
//#define ? sw_gpio_34




//=============================
//======= configuration =======
//=============================
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
//----- stepper config -----
//--------------------------
//enable stepper test mode (dont start control and encoder task)
//#define STEPPER_TEST
#define STEPPER_STEP_PIN GPIO_NUM_18    //mos1
#define STEPPER_DIR_PIN GPIO_NUM_16     //ST3
#define STEPPER_EN_PIN GPIO_NUM_0       //not connected (-> stepper always on)
//more detailed options for testing are currently defined in guide-stepper.cpp



//--------------------------
//------ calibration -------
//--------------------------
//enable mode  encoder test for calibration
//if defined, displays always show length and steps instead of the normal messages
//#define ENCODER_TEST

//steps per meter
#define ENCODER_STEPS_PER_METER 2127 //roll-v3-gummi-86.6mm - d=89.8mm

//millimetres added to target length
//to ensure that length does not fall short when spool slightly rotates back after stop
#define TARGET_LENGTH_OFFSET 0

//millimetres lengthNow can be below lengthTarget to still stay in target_reached state
#define TARGET_REACHED_TOLERANCE 5




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
