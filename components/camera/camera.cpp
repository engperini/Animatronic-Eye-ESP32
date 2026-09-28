#include "eye_camera.hpp"
#include "core.hpp"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"
#include "freertos/task.h"
#include <memory>
static esp_err_t initialize_camera() {
    if(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)<2*1024*1024) return ESP_ERR_NO_MEM;
    camera_config_t config={};
    config.pin_pwdn=-1; config.pin_reset=-1;
    config.pin_xclk=10; config.pin_sccb_sda=40; config.pin_sccb_scl=39;
    config.pin_d0=15; config.pin_d1=17; config.pin_d2=18; config.pin_d3=16;
    config.pin_d4=14; config.pin_d5=12; config.pin_d6=11; config.pin_d7=48;
    config.pin_vsync=38; config.pin_href=47; config.pin_pclk=13;
    config.xclk_freq_hz=20000000;
    config.ledc_timer=LEDC_TIMER_0; config.ledc_channel=LEDC_CHANNEL_0;
    config.pixel_format=PIXFORMAT_RGB565; config.frame_size=FRAMESIZE_QVGA;
    config.fb_location=CAMERA_FB_IN_PSRAM; config.fb_count=1;
    config.grab_mode=CAMERA_GRAB_WHEN_EMPTY;
    return esp_camera_init(&config);
}
static void camera_task(void *) {
    bool initialized=false;
    std::unique_ptr<HumanFaceDetect> detector;
    while(true) {
        bool requested;
        { StateLock lock; requested=eye.camera_requested; }
        if(!requested) {
            if(initialized) { detector.reset(); esp_camera_deinit(); initialized=false; }
            { StateLock lock; eye.camera_ready=false; eye.face=false; }
            vTaskDelay(pdMS_TO_TICKS(100)); continue;
        }
        if(!initialized) {
            esp_err_t result=initialize_camera();
            if(result==ESP_OK) { detector=std::make_unique<HumanFaceDetect>(); initialized=true; }
            { StateLock lock; eye.camera_error=result; eye.camera_ready=initialized; if(!initialized) eye.camera_requested=false; }
            if(!initialized) continue;
        }
        camera_fb_t *frame=esp_camera_fb_get();
        bool found=false;
        float face_x=0,face_y=0;
        if(frame) {
            dl::image::img_t image={};
            image.data=frame->buf; image.width=frame->width; image.height=frame->height;
            image.pix_type=dl::image::DL_IMAGE_PIX_TYPE_RGB565;
            auto &results=detector->run(image);
            int largest=0;
            for(const auto &face:results) {
                int area=(face.box[2]-face.box[0])*(face.box[3]-face.box[1]);
                if(area>largest) {
                    largest=area; found=true;
                    face_x=bounded(static_cast<float>(face.box[0]+face.box[2])/frame->width-1);
                    face_y=bounded(static_cast<float>(face.box[1]+face.box[3])/frame->height-1);
                }
            }
            esp_camera_fb_return(frame);
        }
        {
            StateLock lock;
            eye.face=found && eye.camera_requested;
            eye.face_x=face_x; eye.face_y=face_y;
            eye.face_time=esp_timer_get_time();
            eye.camera_error=frame ? ESP_OK : ESP_FAIL;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
esp_err_t eye_camera_start() {
    return xTaskCreate(camera_task,"camera",8192,nullptr,3,nullptr)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
