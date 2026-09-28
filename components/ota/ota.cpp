#include "eye_ota.hpp"
#include "core.hpp"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "freertos/task.h"
static void restart_task(void *) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}
void eye_restart_later() { xTaskCreate(restart_task,"restart",2048,nullptr,2,nullptr); }
esp_err_t eye_ota_confirm() {
    esp_ota_img_states_t state;
    const esp_partition_t *running=esp_ota_get_running_partition();
    if(esp_ota_get_state_partition(running,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY)
        return esp_ota_mark_app_valid_cancel_rollback();
    return ESP_OK;
}
esp_err_t eye_ota_upload(httpd_req_t *request) {
    const esp_partition_t *partition=esp_ota_get_next_update_partition(nullptr);
    if(!partition || request->content_len<1024 || static_cast<size_t>(request->content_len)>partition->size)
        return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid firmware size");
    { StateLock lock; eye.armed=false; eye.camera_requested=false; eye.mode=Mode::Manual; }
    esp_ota_handle_t handle;
    esp_err_t result=esp_ota_begin(partition,request->content_len,&handle);
    if(result!=ESP_OK) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,esp_err_to_name(result));
    char buffer[4096];
    int remaining=request->content_len, timeouts=0;
    while(remaining>0) {
        int received=httpd_req_recv(request,buffer,remaining>sizeof(buffer) ? sizeof(buffer) : remaining);
        if(received==HTTPD_SOCK_ERR_TIMEOUT && ++timeouts<=3) continue;
        if(received<=0) { esp_ota_abort(handle); return ESP_FAIL; }
        timeouts=0;
        result=esp_ota_write(handle,buffer,received);
        if(result!=ESP_OK) { esp_ota_abort(handle); return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,esp_err_to_name(result)); }
        remaining-=received;
    }
    result=esp_ota_end(handle);
    if(result==ESP_OK) result=esp_ota_set_boot_partition(partition);
    if(result!=ESP_OK) return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,esp_err_to_name(result));
    httpd_resp_sendstr(request,"Firmware accepted. Restarting.");
    eye_restart_later();
    return ESP_OK;
}
