#include "display.hpp"


//=== variables ===
static const char *TAG = "display"; //tag for logging



//==============================
//======== init display ========
//==============================
//initialize display with parameters defined in config.hpp
//TODO: dont use global variables/macros here
max7219_t display_init(){
    
    ESP_LOGI(TAG, "initializing display...");
    // Configure SPI bus
    spi_bus_config_t cfg;
    memset(&cfg, 0, sizeof(spi_bus_config_t)); //init bus config with 0 to prevent bugs with random flags
    cfg.mosi_io_num = DISPLAY_PIN_NUM_MOSI;
    cfg.miso_io_num = -1;
    cfg.sclk_io_num = DISPLAY_PIN_NUM_CLK;
    cfg.quadwp_io_num = -1;
    cfg.quadhd_io_num = -1;
    cfg.max_transfer_sz = 0;
    cfg.flags = 0;
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    // Configure device
    max7219_t dev;
    dev.cascade_size = 2;
    dev.digits = 0;
    dev.mirrored = true;
    ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, MAX7219_MAX_CLOCK_SPEED_HZ, DISPLAY_PIN_NUM_CS));
    ESP_ERROR_CHECK(max7219_init(&dev));
    //0...15
    ESP_ERROR_CHECK(max7219_set_brightness(&dev, 8));
    return dev;
    //display = dev;
    ESP_LOGI(TAG, "initializing display - done");
}



//===================================
//======= display welcome msg =======
//===================================
void display_ShowWelcomeMsg(max7219_t dev){
    //display welcome message on two 7 segment displays
    //show name and date
    ESP_LOGI(TAG, "showing startup message...");
    max7219_clear(&dev);
    max7219_draw_text_7seg(&dev, 0, "CUTTER  20.08.2022");
    //                                   1234567812 34 5678
    vTaskDelay(pdMS_TO_TICKS(700));
    //scroll "hello" over 2 displays
    for (int offset = 0; offset < 23; offset++) {
        max7219_clear(&dev);
        char hello[40] = "                HELL0                 ";
        max7219_draw_text_7seg(&dev, 0, hello + (22 - offset) );
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}





//---------------------------------
//---------- constructor ----------
//---------------------------------
handledDisplay::handledDisplay(max7219_t displayDevice, uint8_t posStart_f) {
    ESP_LOGI(TAG, "Creating handledDisplay instance with startPos at %i", posStart);
    //copy variables
    dev = displayDevice;
    posStart = posStart_f;
}



//--------------------------------
//---------- showString ----------
//--------------------------------
//function that displays a given string on the display
void handledDisplay::showString(const char * buf, uint8_t pos_f){
    //calculate actual absolute position
    posCurrent = posStart + pos_f;
    //copy the desired string
    strcpy(strOn, buf);
    //exit blinking mode
    if (mode == displayMode::BLINK_STRINGS){
        mode = displayMode::NORMAL;
        ESP_LOGI(TAG, "pos:%i - disable blink strings mode -> normal mode str='%s'", posStart, strOn);
    }
    handle(); //draws the text depending on mode
}



//TODO: blinkStrings() and blink() are very similar - can be optimized?
//only difficulty currently is the reset behaivor of blinkStrings through showString (blink does not reset)

//----------------------------------
//---------- blinkStrings ----------
//----------------------------------
//function switches between two strings in a given interval
void handledDisplay::blinkStrings(const char * strOn_f, const char * strOff_f, uint32_t msOn_f, uint32_t msOff_f){
    //copy/update variables
    strcpy(strOn, strOn_f);
    strcpy(strOff, strOff_f);
    msOn = msOn_f;
    msOff = msOff_f;
    //if changed to blink mode just now:
    if (mode != displayMode::BLINK_STRINGS) {
        //switch mode
        ESP_LOGI(TAG, "pos:%i - toggle blink strings mode on/off=%d/%d stings='%s'/'%s'", posStart, msOn, msOff, strOn, strOff);
        mode = displayMode::BLINK_STRINGS;
        //start with on state
        state = true;
        timestampOn = esp_log_timestamp();
    }
    //run handle function for display update
    handle();
}



//-------------------------------
//------------ blink ------------
//-------------------------------
//function triggers certain count and interval of off durations
void handledDisplay::blink(uint8_t count_f, uint32_t msOn_f, uint32_t msOff_f, const char * strOff_f) {
    //copy/update parameters
    count = count_f;
    msOn = msOn_f;
    msOff = msOff_f;
    strcpy(strOff, strOff_f);
    //FIXME this strings length must be dynamic depending on display size (posEnd - posStart) -> otherwise overwrites next segments if other display size or start pos
    //if changed to blink mode just now:
    if (mode != displayMode::BLINK) {
        //set to blink mode
        mode = displayMode::BLINK;
        ESP_LOGI(TAG, "pos:%i - start blinking: count=%i  on/off=%d/%d sting='%s'",posStart, count, msOn, msOff, strOff);
        //start with off state
        state = false;
        timestampOff = esp_log_timestamp();
    }
    //run handle function for display update
    handle();
}



//--------------------------------
//------------ handle ------------
//--------------------------------
//function that handles time based modes
//writes text to the 7 segment display depending on the current mode
void handledDisplay::handle() {
    switch (mode){
        case displayMode::NORMAL:
            //daw given string
            max7219_draw_text_7seg(&dev, posCurrent, strOn);
            break;

        case displayMode::BLINK:
        case displayMode::BLINK_STRINGS:
            //--- define state on/off ---
            if (state == true){ //display in ON state
                if (esp_log_timestamp() - timestampOn > msOn){
                    state = false;
                    timestampOff = esp_log_timestamp();
                    //decrement remaining counts in BLINK mode each cycle
                    if (mode == displayMode::BLINK) count--;
                }
            } else { //display in OFF state
                if (esp_log_timestamp() - timestampOff > msOff) {
                    state = true;
                    timestampOn = esp_log_timestamp();
                }
            }
            //--- draw text of current state ---
            if (state) {
                max7219_draw_text_7seg(&dev, posStart, strOn);
            } else {
                max7219_draw_text_7seg(&dev, posStart, strOff);
            }

            //--- check finished condition in BLINK mode ---
            if (mode == displayMode::BLINK){
                if (count == 0) {
                    mode = displayMode::NORMAL;
                    ESP_LOGI(TAG, "pos:%i - finished blinking -> normal mode", posStart);
                }
            }
            break;
    }
}
