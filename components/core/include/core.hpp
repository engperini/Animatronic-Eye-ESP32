#pragma once
#include <cmath>
#include <cstdint>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct AxisCalibration { int minimum=1300, center=1500, maximum=1700; bool inverted=false; };
struct Calibration {
    uint32_t version=1;
    AxisCalibration x, y;
    int lid_open=1500, lid_closed=1700;
    bool lid_enabled=false;
};
enum class Mode { Manual, Random, Tracking };
struct EyeState {
    Calibration calibration;
    Mode mode=Mode::Manual;
    float x=0, y=0, target_x=0, target_y=0, lid=0;
    float smoothing=0.12f, speed=0.8f, deadband=0.08f;
    bool armed=false, camera_requested=false, camera_ready=false, face=false;
    float face_x=0, face_y=0;
    int64_t face_time=0;
    int64_t control_heartbeat=0;
    esp_err_t camera_error=ESP_OK;
};
extern EyeState eye;
extern SemaphoreHandle_t state_mutex;
struct StateLock {
    StateLock() { xSemaphoreTakeRecursive(state_mutex, portMAX_DELAY); }
    ~StateLock() { xSemaphoreGiveRecursive(state_mutex); }
};
inline float bounded(float value, float low=-1, float high=1) { return fminf(high, fmaxf(low,value)); }
