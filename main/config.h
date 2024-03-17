#pragma once

#include "esp_idf_version.h"

//note: global variables and objects were moved to global.hpp

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

#define ADC_CHANNEL ADC_CHANNEL_0
//#define ADC_LOW_VOLTAGE_THRESHOLD 1000 //adc value where shut down is detected (store certain values before power loss)
#define GPIO_PIN GPIO_NUM_2

#define ADC_CHANNEL_SUPPLY_VOLTAGE ADC1_CHANNEL_7//gpio35 onboard supply voltage
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
//#define ? sw_gpio_34
//note: actual objects are created in global.cpp




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
#define DISPLAY_BRIGHTNESS 8

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
//pins
#define STEPPER_STEP_PIN		GPIO_NUM_18    //mos1
#define STEPPER_DIR_PIN			GPIO_NUM_16     //ST3
//driver settings
#define STEPPER_STEPS_PER_MM	(200/2)	//steps/mm (steps-per-rot / spindle-slope)
#define STEPPER_SPEED_DEFAULT	25		//mm/s
#define STEPPER_SPEED_MIN		4		//mm/s  - speed threshold at which stepper immediately starts/stops
#define STEPPER_ACCEL_INC		3		//steps/s increment per cycle 
#define STEPPER_DECEL_INC		7		//steps/s decrement per cycle
//options affecting movement are currently defined in guide-stepper.cpp


//---------------------------
//------- cable guide -------
//---------------------------
// default axis coordinates the guide changes direction (winding width)
#define GUIDE_MIN_MM 0  // TODO add feature so guide stays at zero for some steps (negative MIN_MM?), currently seems appropriate for even winding
#define GUIDE_MAX_MM 90 // 95 still to long at max pos - actual reel is 110, but currently guide turned out to stay at max position for too long, due to cable running diagonal from guide to reel

// tolerance added to last stored position at previous shutdown.
// When calibrating at startup the stepper moves for that sum to get track of zero position (ensure crashes into hardware limit for at least some time)
#define AUTO_HOME_TRAVEL_ADD_TO_LAST_POS_MM 20
#define MAX_TOTAL_AXIS_TRAVEL_MM 103 // max possible travel distance, needed as fallback for auto-home
#define LAYER_THICKNESS_MM 5         // height of one cable layer on reel -> increase in radius every layer
#define D_CABLE 6                    // determines winds per layer / guide speed
#define D_REEL 160                   // start diameter of empty reel

// max winding width that can be set using potentiometer (SET+PRESET1 buttons)
#define MAX_SELECTABLE_WINDING_WIDTH_MM 100;
// max target length that can be selected using potentiometer (SET button)
#define MAX_SELECTABLE_LENGTH_POTI_MM 100000

// calculate new winding width each time target length changes, according to custom thresholds defined in guide-stepper.cpp
// if not defined, winding width is always GUIDE_MAX_MM even for short lengths
#define DYNAMIC_WINDING_WIDTH_ENABLED


//--------------------------
//------ calibration -------
//--------------------------
//enable mode  encoder test for calibration (determine ENCODER_STEPS_PER_METER)
//if defined, displays always show length and steps instead of the normal messages
//#define ENCODER_TEST
//TODO: add way to calibrate without flashing -> enter calibration mode with certain button sequence, enter steps-per-meter with poti, store in nvs

//steps per meter
//this value is determined experimentally while ENCODER_TEST is enabled
//#define ENCODER_STEPS_PER_METER 2127 //until 2024.03.13 roll-v3-gummi-86.6mm - d=89.8mm
#define ENCODER_STEPS_PER_METER 2118 //2024.03.13 roll-v3-gummi measured 86.5mm

//millimeters added to target length
//to ensure that length does not fall short when spool slightly rotates back after stop
#define TARGET_LENGTH_OFFSET 0

//millimeters lengthNow can be below lengthTarget to still stay in target_reached state
#define TARGET_REACHED_TOLERANCE 5




