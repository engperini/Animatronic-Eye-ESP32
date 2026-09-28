#include "eye_control.hpp"
#include "core.hpp"
#include "servo.hpp"
#include "calibration.hpp"
#include "tracking.hpp"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/task.h"
static void control_task(void *) {
    TickType_t wake=xTaskGetTickCount();
    int64_t next_random=0;
    int64_t next_blink=0;
    int64_t blink_started=-1;
    while(true) {
        {
            StateLock lock;
            eye.control_heartbeat=esp_timer_get_time();
            const int64_t now=eye.control_heartbeat;
            if(eye.armed && eye.mode==Mode::Random && eye.calibration.lid_enabled) {
                if(next_blink==0) next_blink=now+2000000+esp_random()%4000000;
                if(blink_started<0 && now>=next_blink) blink_started=now;
                if(blink_started>=0) {
                    const int64_t elapsed=now-blink_started;
                    if(elapsed<120000) eye.lid=static_cast<float>(elapsed)/120000;
                    else if(elapsed<180000) eye.lid=1;
                    else if(elapsed<360000) eye.lid=1-static_cast<float>(elapsed-180000)/180000;
                    else {
                        eye.lid=0;
                        blink_started=-1;
                        next_blink=now+2000000+esp_random()%4000000;
                    }
                } else eye.lid=fmaxf(0,eye.lid-0.1f);
            } else {
                next_blink=0;
                blink_started=-1;
            }
            if(eye.armed) {
                if(eye.mode==Mode::Random && esp_timer_get_time()>next_random) {
                    eye.target_x=(static_cast<int>(esp_random()%1801)-900)/1000.0f;
                    eye.target_y=(static_cast<int>(esp_random()%1401)-700)/1000.0f;
                    next_random=esp_timer_get_time()+700000+esp_random()%2300000;
                }
                if(eye.mode==Mode::Tracking) tracking_update();
                eye.x=tracking_step(eye.x,eye.target_x,eye.smoothing,eye.speed,0.02f);
                eye.y=tracking_step(eye.y,eye.target_y,eye.smoothing,eye.speed,0.02f);
                ESP_ERROR_CHECK(servo_write(0,calibration_pulse(eye.calibration.x,eye.x)));
                ESP_ERROR_CHECK(servo_write(1,calibration_pulse(eye.calibration.y,eye.y)));
                if(eye.calibration.lid_enabled)
                    ESP_ERROR_CHECK(servo_write(2,lroundf(eye.calibration.lid_open+eye.lid*(eye.calibration.lid_closed-eye.calibration.lid_open))));
                else servo_stop(2);
            } else for(int channel=0;channel<3;channel++) servo_stop(channel);
        }
        vTaskDelayUntil(&wake,pdMS_TO_TICKS(20));
    }
}
esp_err_t eye_control_start() {
    return xTaskCreate(control_task,"eye_control",4096,nullptr,5,nullptr)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
