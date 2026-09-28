#include "tracking.hpp"
#include "core.hpp"
#include "esp_timer.h"
float tracking_step(float position,float target,float smoothing,float speed,float seconds) {
    float delta=(target-position)*(1-powf(1-smoothing,seconds/0.02f));
    return bounded(position+bounded(delta,-speed*seconds,speed*seconds));
}
void tracking_update() {
    if(!eye.camera_ready || !eye.face || esp_timer_get_time()-eye.face_time>600000) {
        eye.target_x=eye.x; eye.target_y=eye.y;
        return;
    }
    if(fabsf(eye.face_x)>eye.deadband) eye.target_x=bounded(eye.x+eye.face_x*0.25f);
    else eye.target_x=eye.x;
    if(fabsf(eye.face_y)>eye.deadband) eye.target_y=bounded(eye.y+eye.face_y*0.25f);
    else eye.target_y=eye.y;
}
