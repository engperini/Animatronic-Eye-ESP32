#include "servo.hpp"
#include "driver/ledc.h"
#include "sdkconfig.h"
static bool valid_pin(int pin) {
    const int available[]={1,2,4,5,6,7,8,9,43,44};
    for(int candidate:available) if(pin==candidate) return true;
    return false;
}
esp_err_t servo_init() {
    if(!valid_pin(CONFIG_EYE_X_GPIO) || !valid_pin(CONFIG_EYE_Y_GPIO) || !valid_pin(CONFIG_EYE_LID_GPIO)) return ESP_ERR_INVALID_ARG;
    if(CONFIG_EYE_X_GPIO==CONFIG_EYE_Y_GPIO || CONFIG_EYE_X_GPIO==CONFIG_EYE_LID_GPIO || CONFIG_EYE_Y_GPIO==CONFIG_EYE_LID_GPIO) return ESP_ERR_INVALID_ARG;
    ledc_timer_config_t timer={};
    timer.speed_mode=LEDC_LOW_SPEED_MODE;
    timer.duty_resolution=LEDC_TIMER_14_BIT;
    timer.timer_num=LEDC_TIMER_1;
    timer.freq_hz=50;
    timer.clk_cfg=LEDC_AUTO_CLK;
    esp_err_t result=ledc_timer_config(&timer);
    if(result!=ESP_OK) return result;
    const int pins[]={CONFIG_EYE_X_GPIO,CONFIG_EYE_Y_GPIO,CONFIG_EYE_LID_GPIO};
    for(int channel=0;channel<3;channel++) {
        ledc_channel_config_t config={};
        config.gpio_num=pins[channel];
        config.speed_mode=LEDC_LOW_SPEED_MODE;
        config.channel=static_cast<ledc_channel_t>(channel+1);
        config.timer_sel=LEDC_TIMER_1;
        result=ledc_channel_config(&config);
        if(result!=ESP_OK) return result;
    }
    return ESP_OK;
}
esp_err_t servo_write(int channel,int microseconds) {
    if(channel<0 || channel>2 || microseconds<500 || microseconds>2500) return ESP_ERR_INVALID_ARG;
    return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE,static_cast<ledc_channel_t>(channel+1),
        static_cast<uint32_t>(microseconds)*16384/20000,0);
}
void servo_stop(int channel) {
    ledc_stop(LEDC_LOW_SPEED_MODE,static_cast<ledc_channel_t>(channel+1),0);
}
