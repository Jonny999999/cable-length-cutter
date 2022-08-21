#include "display.hpp"


//=== variables ===
static const char *TAG = "display"; //tag for logging
max7219_t display;

//========================
//===== init display =====
//========================
void display_init(){
    
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
    ESP_ERROR_CHECK(max7219_set_brightness(&dev, 12));
    //return dev;
    display = dev;
    ESP_LOGI(TAG, "initializing display - done");
}



void display_ShowWelcomeMsg(){
    //-----------------------------------
    //------- display welcome msg -------
    //-----------------------------------
    //display welcome message on two 7 segment displays
    //show name and date
    ESP_LOGI(TAG, "showing startup message...");
    max7219_clear(&display);
    max7219_draw_text_7seg(&display, 0, "CUTTER  20.08.2022");
    //                                   1234567812 34 5678
    vTaskDelay(pdMS_TO_TICKS(700));
    //scroll "hello" over 2 displays
    for (int offset = 0; offset < 23; offset++) {
        max7219_clear(&display);
        char hello[40] = "                HELL0                 ";
        max7219_draw_text_7seg(&display, 0, hello + (22 - offset) );
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}



void display1_showString(const char * buf){
        max7219_draw_text_7seg(&display, 0, buf);
}
void display2_showString(const char * buf){
        max7219_draw_text_7seg(&display, 8, buf);
}

void display_showString(uint8_t pos, const char * buf){
        max7219_draw_text_7seg(&display, pos, buf);
}

//        //---------------------------
//        //--------- display ---------
//        //---------------------------
//        //-- show current position on display1 ---
//        //sprintf(buf_tmp, "%06.1f cm", (float)lengthNow/10); //cm
//        sprintf(buf_tmp, "1ST %5.4f", (float)lengthNow/1000); //m
//        //                123456789
//        //limit length to 8 digits + decimal point (drop decimal places when it does not fit)
//        sprintf(buf_disp1, "%.9s", buf_tmp);
//
//        //--- show target length on display2 ---
//        //sprintf(buf_disp2, "%06.1f cm", (float)lengthTarget/10); //cm
//        sprintf(buf_disp2, "S0LL%5.3f", (float)lengthTarget/1000); //m
//        //                  1234  5678
//        
//                //TODO: blink disp2 when set button pressed
//        //TODO: blink disp2 when preset button pressed (exept manual mode)
//        //TODO: write "MAN CTL" to disp2 when in manual mode
//        //TODO: display or blink "REACHED" when reached state and start pressed
//
//        //--- write to display ---
//        //max7219_clear(&display); //results in flickering display if same value anyways
//        max7219_draw_text_7seg(&display, 0, buf_disp1);
//        max7219_draw_text_7seg(&display, 8, buf_disp2);
//
//
//
