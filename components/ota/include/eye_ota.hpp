#pragma once
#include "esp_http_server.h"
esp_err_t eye_ota_upload(httpd_req_t *request);
void eye_restart_later();
esp_err_t eye_ota_confirm();
