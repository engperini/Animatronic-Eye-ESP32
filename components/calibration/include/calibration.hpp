#pragma once
#include "core.hpp"
bool calibration_valid(const Calibration &value);
esp_err_t calibration_load();
esp_err_t calibration_save(const Calibration &value);
int calibration_pulse(const AxisCalibration &axis, float position);
