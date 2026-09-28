#pragma once
#include "esp_err.h"
esp_err_t servo_init();
esp_err_t servo_write(int channel,int microseconds);
void servo_stop(int channel);
