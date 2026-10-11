#include "pca9685.hpp"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static constexpr i2c_port_t PORT = I2C_NUM_0;
static constexpr uint8_t ADDRESS = 0x40;
static constexpr uint8_t MODE1 = 0x00;
static constexpr uint8_t PRESCALE = 0xFE;
static constexpr uint8_t LED0 = 0x06;

static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    uint8_t bytes[] = {reg, value};
    return i2c_master_write_to_device(PORT, ADDRESS, bytes, sizeof(bytes), pdMS_TO_TICKS(100));
}
static esp_err_t write_channel(uint8_t channel, uint16_t off, bool full_off) {
    if (channel >= 16) return ESP_ERR_INVALID_ARG;
    uint8_t bytes[] = {static_cast<uint8_t>(LED0 + 4 * channel), 0, 0,
                       static_cast<uint8_t>(off & 0xff),
                       static_cast<uint8_t>(((off >> 8) & 0x0f) | (full_off ? 0x10 : 0))};
    return i2c_master_write_to_device(PORT, ADDRESS, bytes, sizeof(bytes), pdMS_TO_TICKS(100));
}
esp_err_t pca9685_init() {
    i2c_config_t cfg = {};
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = static_cast<gpio_num_t>(CONFIG_EYE_PCA_SDA_GPIO);
    cfg.scl_io_num = static_cast<gpio_num_t>(CONFIG_EYE_PCA_SCL_GPIO);
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = 100000;
    esp_err_t err = i2c_param_config(PORT, &cfg);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(PORT, cfg.mode, 0, 0, 0);
    if (err != ESP_OK) return err;
    // Sleep before setting the prescaler; 25 MHz oscillator, 50 Hz output.
    err = write_reg(MODE1, 0x10);
    if (err == ESP_OK) err = write_reg(PRESCALE, 121);
    if (err == ESP_OK) err = write_reg(MODE1, 0x20);
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = write_reg(MODE1, 0xA0);
    }
    if (err == ESP_OK) {
        for (uint8_t ch = 0; ch < 16 && err == ESP_OK; ++ch) err = pca9685_stop(ch);
    }
    if (err != ESP_OK) i2c_driver_delete(PORT);
    return err;
}
esp_err_t pca9685_write(uint8_t channel, uint16_t pulse_us) {
    if (pulse_us < 500 || pulse_us > 2500) return ESP_ERR_INVALID_ARG;
    // 20 ms frame; rounded to nearest PCA9685 tick.
    uint16_t ticks = static_cast<uint16_t>((static_cast<uint32_t>(pulse_us) * 4096 + 10000) / 20000);
    return write_channel(channel, ticks, false);
}
esp_err_t pca9685_stop(uint8_t channel) { return write_channel(channel, 0, true); }
