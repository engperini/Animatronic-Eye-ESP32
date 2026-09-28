#pragma once
#include "esp_err.h"
esp_err_t eye_wifi_start();
esp_err_t eye_wifi_save(const char *ssid,const char *password);
const char *eye_wifi_password();
const char *eye_wifi_ap_name();
