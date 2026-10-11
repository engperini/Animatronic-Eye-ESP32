#include "servo.hpp"
#include "pca9685.hpp"

esp_err_t servo_init() {
    return pca9685_init();
}
esp_err_t servo_write(int channel, int microseconds) {
    if (channel < 0 || channel > 2 || microseconds < 500 || microseconds > 2500)
        return ESP_ERR_INVALID_ARG;
    return pca9685_write(static_cast<uint8_t>(channel), static_cast<uint16_t>(microseconds));
}
void servo_stop(int channel) {
    if (channel < 0 || channel > 2) return;
    (void)pca9685_stop(static_cast<uint8_t>(channel));
}
