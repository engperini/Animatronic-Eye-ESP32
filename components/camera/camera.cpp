#include "eye_camera.hpp"
#include "core.hpp"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "human_face_detect.hpp"
#include "freertos/task.h"
#include "esp_log.h"
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
    uint8_t hits=0, misses=0;
    bool stable_face=false;
    int64_t last_seen=0;
    int64_t last_log=0;
    uint32_t frames=0, detections=0, failures=0;
    while(true) {
        bool requested;
        { StateLock lock; requested=eye.camera_requested; }
        if(!requested) {
            if(initialized) { detector.reset(); esp_camera_deinit(); initialized=false; }
            hits=misses=0; stable_face=false; last_seen=0;
            { StateLock lock; eye.camera_ready=false; eye.face=false; eye.face_hits=0; eye.face_misses=0; }
            vTaskDelay(pdMS_TO_TICKS(100)); continue;
        }
        if(!initialized) {
            esp_err_t result=initialize_camera();
            if(result==ESP_OK) { detector=std::make_unique<HumanFaceDetect>(); initialized=true; }
            { StateLock lock; eye.camera_error=result; eye.camera_ready=initialized; if(!initialized) eye.camera_requested=false; }
            if(!initialized) continue;
        }
        const int64_t started=esp_timer_get_time();
        camera_fb_t *frame=esp_camera_fb_get();
        bool found=false;
        float face_x=0,face_y=0;
        ++frames;
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
        } else ++failures;
        const int64_t now=esp_timer_get_time();
        if(found) {
            ++detections;
            hits=static_cast<uint8_t>(hits<3?hits+1:3);
            misses=0;
            last_seen=now;
            if(hits>=2) stable_face=true;
        } else {
            hits=0;
            misses=static_cast<uint8_t>(misses<255?misses+1:255);
            if(stable_face && (now-last_seen>1000000 || misses>=12)) stable_face=false;
        }
        if(now-last_log>=5000000) {
            ESP_LOGI("eye_camera","frames=%lu detections=%lu capture_failures=%lu face=%d hits=%u misses=%u processing_ms=%lu heap=%lu psram=%lu",
                static_cast<unsigned long>(frames),static_cast<unsigned long>(detections),static_cast<unsigned long>(failures),
                stable_face, hits, misses,static_cast<unsigned long>((now-started)/1000),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
                static_cast<unsigned long>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
            last_log=now;
        }
        {
            StateLock lock;
            eye.face=stable_face && eye.camera_requested;
            if(found) { eye.face_x=face_x; eye.face_y=face_y; eye.face_time=now; }
            eye.camera_error=frame ? ESP_OK : ESP_FAIL;
            eye.camera_frames=frames; eye.camera_detections=detections; eye.camera_capture_failures=failures;
            eye.camera_processing_ms=static_cast<uint32_t>((now-started)/1000);
            eye.camera_free_heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            eye.camera_free_psram=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            eye.camera_uptime_ms=static_cast<uint32_t>(now/1000);
            eye.face_hits=hits; eye.face_misses=misses;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
esp_err_t eye_camera_start() {
    return xTaskCreate(camera_task,"camera",8192,nullptr,3,nullptr)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
