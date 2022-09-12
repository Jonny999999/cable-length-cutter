#include "switchesAnalog.hpp"

#define CHECK_BIT(var,pos) (((var)>>(pos)) & 1) //TODO duplicate code: same macro already used in vfd.cpp



//=============================
//========= readAdc ===========
//=============================
//TODO add this to gpio library - used multiple times in this project
//function for multisampling an anlog input
int readAdc(adc1_channel_t adc_channel, bool inverted = false) {
    //make multiple measurements
    int adc_reading = 0;
    for (int i = 0; i < 16; i++) {
        adc_reading += adc1_get_raw(adc_channel);
    }
    adc_reading = adc_reading / 16;
    //return original or inverted result
    if (inverted) {
        return 4095 - adc_reading;
    } else {
        return adc_reading;
    }
}





//=====================
//===== Variables =====
//=====================
static const char *TAG = "switches-analog"; //tag for logging
int diffMin = 4095;
int adcValue;
int match_index = 0;

//array that describes voltages for all combinations of the 4 inputs
const int lookup_voltages[] = {
    //ADC, S3 S2 S1 S0
    4095, //0000
    3642, //0001
    3243, //0010
    2887, //0011
    2628, //0100
    2413, //0101
    2274, //0110
    2112, //0111
    1864, //1000
    1748, //1001
    1671, //1010
    1579, //1011
    1488, //1100
    1418, //1101
    1360, //1110
    1294  //1111
};




//===========================
//===== handle function =====
//===========================
//handle demuxing of 4 switches from 1 adc (has to be run repeatedly)
void handle(){
    //read current voltage
    adcValue = readAdc(ADC_CHANNEL_BUTTONS);
    ESP_LOGI(TAG, "voltage read: %d", adcValue);

    //find closest match in lookup table
    diffMin = 4095; //reset diffMin each run
    for (int i=0; i<16; i++){
        int diff = fabs(adcValue - lookup_voltages[i]);
        if (diff < diffMin){
            diffMin = diff;
            match_index = i;
        }
    }

    //get bool values for each input from matched index
    bool s0 = CHECK_BIT(match_index, 0);
    bool s1 = CHECK_BIT(match_index, 1);
    bool s2 = CHECK_BIT(match_index, 2);
    bool s3 = CHECK_BIT(match_index, 3);
    //bool s1 = ((match_index & 0b1000) == 0b1000);
    //bool s2 = ((match_index & 0b0100) == 0b0100);
    //bool s3 = ((match_index & 0b0010) == 0b0010);
    //bool s4 = ((match_index & 0b0001) == 0b0001);


    //log results
    ESP_LOGI(TAG, "adcRead: %d, closest-match: %d, diff: %d, index: %d, switches: %d%d%d%d",
            adcValue, lookup_voltages[match_index], diffMin, match_index, (int)s3, (int)s2, (int)s1, (int)s0);


}



//====================
//===== getState =====
//====================
//get state of certain switch (0-3)
bool switchesAnalog_getState(int swNumber){
    //run handle function to obtain all current input states
    handle();
    //get relevant bit
    bool state = CHECK_BIT(match_index, swNumber);
    ESP_LOGI(TAG, "returned state of switch No. %d = %i", swNumber, (int)state);
    return state;
}

bool switchesAnalog_getState_sw0(){
    return switchesAnalog_getState(0);
}
bool switchesAnalog_getState_sw1(){
    return switchesAnalog_getState(1);
}
bool switchesAnalog_getState_sw2(){
    return switchesAnalog_getState(2);
}
bool switchesAnalog_getState_sw3(){
    return switchesAnalog_getState(3);
}
