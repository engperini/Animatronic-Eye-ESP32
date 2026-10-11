#include "pca9685.hpp"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static constexpr i2c_port_num_t PORT = I2C_NUM_0;
static constexpr uint16_t ADDRESS = 0x40;
static constexpr uint8_t MODE1 = 0x00;
static constexpr uint8_t PRESCALE = 0xFE;
static constexpr uint8_t LED0 = 0x06;
static i2c_master_bus_handle_t bus = nullptr;
static i2c_master_dev_handle_t device = nullptr;

static esp_err_t transmit(const uint8_t *bytes, size_t size) {
    if (device == nullptr) return ESP_ERR_INVALID_STATE;
    return i2c_master_transmit(device, bytes, size, 100);
}
static esp_err_t write_reg(uint8_t reg, uint8_t value) {
    uint8_t bytes[] = {reg, value};
    return transmit(bytes, sizeof(bytes));
}
static esp_err_t write_channel(uint8_t channel, uint16_t off, bool full_off) {
    if (channel >= 16) return ESP_ERR_INVALID_ARG;
    uint8_t bytes[] = {static_cast<uint8_t>(LED0 + 4 * channel), 0, 0,
                       static_cast<uint8_t>(off & 0xff),
                       static_cast<uint8_t>(((off >> 8) & 0x0f) | (full_off ? 0x10 : 0))};
    return transmit(bytes, sizeof(bytes));
}
esp_err_t pca9685_init() {
    if (device != nullptr) return ESP_OK;
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = PORT;
    cfg.sda_io_num = static_cast<gpio_num_t>(CONFIG_EYE_PCA_SDA_GPIO);
    cfg.scl_io_num = static_cast<gpio_num_t>(CONFIG_EYE_PCA_SCL_GPIO);
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.flags.enable_internal_pullup = true;
    esp_err_t err = i2c_new_master_bus(&cfg, &bus);
    if (err != ESP_OK) return err;

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = ADDRESS;
    dev_cfg.scl_speed_hz = 100000;
    err = i2c_master_bus_add_device(bus, &dev_cfg, &device);
    if (err != ESP_OK) {
        i2c_del_master_bus(bus);
        bus = nullptr;
        return err;
    }

    // Preserve the existing PCA9685 50 Hz initialization sequence.
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
    if (err != ESP_OK) {
        i2c_master_bus_rm_device(device);
        device = nullptr;
        i2c_del_master_bus(bus);
        bus = nullptr;
    }
    return err;
}
esp_err_t pca9685_write(uint8_t channel, uint16_t pulse_us) {
    if (pulse_us < 500 || pulse_us > 2500) return ESP_ERR_INVALID_ARG;
    uint16_t ticks = static_cast<uint16_t>((static_cast<uint32_t>(pulse_us) * 4096 + 10000) / 20000);
    return write_channel(channel, ticks, false);
}
esp_err_t pca9685_stop(uint8_t channel) { return write_channel(channel, 0, true); }
