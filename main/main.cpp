#include "core.hpp"
#include "calibration.hpp"
#include "servo.hpp"
#include "eye_control.hpp"
#include "eye_camera.hpp"
#include "eye_wifi.hpp"
#include "web.hpp"
#include "eye_ota.hpp"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
extern "C" void app_main() {
    state_mutex=xSemaphoreCreateRecursiveMutex();
    ESP_ERROR_CHECK(state_mutex ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(nvs_flash_init());
    esp_err_t calibration_result=calibration_load();
    if(calibration_result!=ESP_OK) ESP_LOGW("eye","Using safe defaults: %s",esp_err_to_name(calibration_result));
    ESP_ERROR_CHECK(servo_init());
    ESP_ERROR_CHECK(eye_control_start());
    ESP_ERROR_CHECK(eye_camera_start());
    ESP_ERROR_CHECK(eye_wifi_start());
    ESP_ERROR_CHECK(web_start());
    vTaskDelay(pdMS_TO_TICKS(10000));
    { StateLock lock; ESP_ERROR_CHECK(esp_timer_get_time()-eye.control_heartbeat<200000 ? ESP_OK : ESP_ERR_TIMEOUT); }
    ESP_ERROR_CHECK(eye_ota_confirm());
    ESP_LOGI("eye","Startup self-test complete; outputs disarmed; camera OFF");
}
