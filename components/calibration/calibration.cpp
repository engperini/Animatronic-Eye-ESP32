#include "calibration.hpp"
#include "nvs.h"
static bool valid_axis(const AxisCalibration &axis) {
    return axis.minimum>=500 && axis.maximum<=2500 && axis.minimum<axis.center && axis.center<axis.maximum;
}
bool calibration_valid(const Calibration &value) {
    return value.version==1 && valid_axis(value.x) && valid_axis(value.y) &&
        value.lid_open>=500 && value.lid_open<=2500 && value.lid_closed>=500 && value.lid_closed<=2500;
}
esp_err_t calibration_load() {
    nvs_handle_t handle;
    esp_err_t result=nvs_open("eye",NVS_READONLY,&handle);
    if(result==ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if(result!=ESP_OK) return result;
    Calibration loaded;
    size_t size=sizeof(loaded);
    result=nvs_get_blob(handle,"calibration",&loaded,&size);
    nvs_close(handle);
    if(result==ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if(result!=ESP_OK) return result;
    if(size!=sizeof(loaded) || !calibration_valid(loaded)) return ESP_ERR_INVALID_ARG;
    StateLock lock;
    eye.calibration=loaded;
    return ESP_OK;
}
esp_err_t calibration_save(const Calibration &value) {
    if(!calibration_valid(value)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t result=nvs_open("eye",NVS_READWRITE,&handle);
    if(result!=ESP_OK) return result;
    result=nvs_set_blob(handle,"calibration",&value,sizeof(value));
    if(result==ESP_OK) result=nvs_commit(handle);
    nvs_close(handle);
    if(result==ESP_OK) { StateLock lock; eye.calibration=value; }
    return result;
}
int calibration_pulse(const AxisCalibration &axis,float position) {
    position=bounded(axis.inverted ? -position : position);
    return lroundf(axis.center+position*(position<0 ? axis.center-axis.minimum : axis.maximum-axis.center));
}
