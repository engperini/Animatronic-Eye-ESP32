#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t pca9685_init();
esp_err_t pca9685_write(uint8_t channel, uint16_t pulse_us);
esp_err_t pca9685_stop(uint8_t channel);
