#include "web.hpp"
#include "core.hpp"
#include "calibration.hpp"
#include "eye_wifi.hpp"
#include "eye_ota.hpp"
#include "esp_http_server.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>
extern const char page_start[] asm("_binary_index_html_start");
static char authorization[96];
static bool authenticate(httpd_req_t *request) {
    char supplied[96]={};
    if(httpd_req_get_hdr_value_str(request,"Authorization",supplied,sizeof(supplied))==ESP_OK &&
       strcmp(supplied,authorization)==0) return true;
    httpd_resp_set_status(request,"401 Unauthorized");
    httpd_resp_set_hdr(request,"WWW-Authenticate","Basic realm=\"Animatronic Eye\"");
    httpd_resp_sendstr(request,"Authentication required");
    return false;
}
static bool mutation_allowed(httpd_req_t *request) {
    char header[8]={};
    if(httpd_req_get_hdr_value_str(request,"X-Eye-Request",header,sizeof(header))==ESP_OK && strcmp(header,"1")==0) return true;
    httpd_resp_send_err(request,HTTPD_403_FORBIDDEN,"Missing request header");
    return false;
}
static esp_err_t send_json(httpd_req_t *request,cJSON *json) {
    char *text=cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if(!text) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"Out of memory");
    httpd_resp_set_type(request,"application/json");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    esp_err_t result=httpd_resp_sendstr(request,text);
    cJSON_free(text);
    return result;
}
static cJSON *axis_json(const AxisCalibration &axis) {
    cJSON *json=cJSON_CreateObject();
    cJSON_AddNumberToObject(json,"min",axis.minimum);
    cJSON_AddNumberToObject(json,"center",axis.center);
    cJSON_AddNumberToObject(json,"max",axis.maximum);
    cJSON_AddBoolToObject(json,"inverted",axis.inverted);
    return json;
}
static esp_err_t status_handler(httpd_req_t *request) {
    if(!authenticate(request)) return ESP_OK;
    EyeState current;
    { StateLock lock; current=eye; }
    cJSON *json=cJSON_CreateObject();
    cJSON_AddBoolToObject(json,"armed",current.armed);
    cJSON_AddStringToObject(json,"mode",current.mode==Mode::Manual ? "manual" : current.mode==Mode::Random ? "random" : "tracking");
    cJSON_AddNumberToObject(json,"x",current.x); cJSON_AddNumberToObject(json,"y",current.y);
    cJSON_AddNumberToObject(json,"lid",current.lid);
    cJSON_AddBoolToObject(json,"camera",current.camera_requested);
    cJSON_AddBoolToObject(json,"camera_ready",current.camera_ready);
    cJSON_AddStringToObject(json,"camera_error",esp_err_to_name(current.camera_error));
    cJSON_AddBoolToObject(json,"face",current.face);
    cJSON_AddNumberToObject(json,"face_x",current.face_x); cJSON_AddNumberToObject(json,"face_y",current.face_y);
    cJSON_AddNumberToObject(json,"smoothing",current.smoothing);
    cJSON_AddNumberToObject(json,"speed",current.speed);
    cJSON_AddNumberToObject(json,"deadband",current.deadband);
    cJSON *calibration=cJSON_AddObjectToObject(json,"calibration");
    cJSON_AddItemToObject(calibration,"x",axis_json(current.calibration.x));
    cJSON_AddItemToObject(calibration,"y",axis_json(current.calibration.y));
    cJSON_AddNumberToObject(calibration,"lid_open",current.calibration.lid_open);
    cJSON_AddNumberToObject(calibration,"lid_closed",current.calibration.lid_closed);
    cJSON_AddBoolToObject(calibration,"lid_enabled",current.calibration.lid_enabled);
    return send_json(request,json);
}
static cJSON *read_json(httpd_req_t *request) {
    if(request->content_len<=0 || request->content_len>2048) return nullptr;
    char buffer[2049];
    int received=0,timeouts=0;
    while(received<request->content_len) {
        int count=httpd_req_recv(request,buffer+received,request->content_len-received);
        if(count==HTTPD_SOCK_ERR_TIMEOUT && ++timeouts<=2) continue;
        if(count<=0) return nullptr;
        received+=count;
    }
    buffer[received]=0;
    return cJSON_ParseWithLengthOpts(buffer,received+1,nullptr,true);
}
static bool number(cJSON *json,const char *name,float &value,float low,float high) {
    cJSON *item=cJSON_GetObjectItemCaseSensitive(json,name);
    if(!item) return true;
    if(!cJSON_IsNumber(item) || !std::isfinite(item->valuedouble) || item->valuedouble<low || item->valuedouble>high) return false;
    value=item->valuedouble;
    return true;
}
static bool boolean(cJSON *json,const char *name,bool &value) {
    cJSON *item=cJSON_GetObjectItemCaseSensitive(json,name);
    if(!item) return true;
    if(!cJSON_IsBool(item)) return false;
    value=cJSON_IsTrue(item); return true;
}
static bool integer(cJSON *json,const char *name,int &value) {
    float parsed=value;
    if(!number(json,name,parsed,500,2500) || parsed!=floorf(parsed)) return false;
    value=parsed; return true;
}
static bool parse_axis(cJSON *json,AxisCalibration &axis) {
    return cJSON_IsObject(json) && integer(json,"min",axis.minimum) && integer(json,"center",axis.center) &&
        integer(json,"max",axis.maximum) && boolean(json,"inverted",axis.inverted);
}
static esp_err_t command_handler(httpd_req_t *request) {
    if(!authenticate(request) || !mutation_allowed(request)) return ESP_OK;
    cJSON *json=read_json(request);
    if(!cJSON_IsObject(json)) { cJSON_Delete(json); return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid JSON"); }
    bool valid=true;
    esp_err_t result=ESP_OK;
    bool restart=false;
    if(strcmp(request->uri,"/api/control")==0) {
        StateLock lock;
        EyeState next=eye;
        valid=number(json,"x",next.target_x,-1,1) && number(json,"y",next.target_y,-1,1) &&
            number(json,"lid",next.lid,0,1) && number(json,"smoothing",next.smoothing,0.01,1) &&
            number(json,"speed",next.speed,0.05,3) && number(json,"deadband",next.deadband,0,0.5) &&
            boolean(json,"armed",next.armed) && boolean(json,"camera",next.camera_requested);
        cJSON *mode=cJSON_GetObjectItemCaseSensitive(json,"mode");
        if(mode) {
            if(!cJSON_IsString(mode)) valid=false;
            else if(strcmp(mode->valuestring,"manual")==0) next.mode=Mode::Manual;
            else if(strcmp(mode->valuestring,"random")==0) next.mode=Mode::Random;
            else if(strcmp(mode->valuestring,"tracking")==0) next.mode=Mode::Tracking;
            else valid=false;
        }
        if(!next.camera_requested && next.mode==Mode::Tracking) {
            if(cJSON_GetObjectItemCaseSensitive(json,"camera")) next.mode=Mode::Manual;
            else valid=false;
        }
        if(next.mode!=eye.mode) {
            if(!cJSON_GetObjectItemCaseSensitive(json,"x")) next.target_x=next.x;
            if(!cJSON_GetObjectItemCaseSensitive(json,"y")) next.target_y=next.y;
        }
        if(!next.armed) { next.target_x=next.x; next.target_y=next.y; }
        if(valid) eye=next;
    } else if(strcmp(request->uri,"/api/calibration")==0) {
        StateLock lock;
        if(eye.armed) {
            cJSON_Delete(json);
            return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Desative os servos antes de alterar a calibracao");
        }
        Calibration next=eye.calibration;
        bool persist=false;
        valid=parse_axis(cJSON_GetObjectItemCaseSensitive(json,"x"),next.x) &&
            parse_axis(cJSON_GetObjectItemCaseSensitive(json,"y"),next.y) &&
            integer(json,"lid_open",next.lid_open) && integer(json,"lid_closed",next.lid_closed) &&
            boolean(json,"lid_enabled",next.lid_enabled) && boolean(json,"persist",persist) && calibration_valid(next);
        if(valid) {
            if(persist) result=calibration_save(next);
            else eye.calibration=next;
            eye.mode=Mode::Manual;
        }
    } else if(strcmp(request->uri,"/api/wifi")==0) {
        cJSON *ssid=cJSON_GetObjectItemCaseSensitive(json,"ssid");
        cJSON *password=cJSON_GetObjectItemCaseSensitive(json,"password");
        valid=cJSON_IsString(ssid) && cJSON_IsString(password);
        if(valid) { result=eye_wifi_save(ssid->valuestring,password->valuestring); restart=result==ESP_OK; }
    } else valid=false;
    cJSON_Delete(json);
    if(!valid || result==ESP_ERR_INVALID_ARG) return httpd_resp_send_err(request,HTTPD_400_BAD_REQUEST,"Invalid parameters");
    if(result!=ESP_OK) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,esp_err_to_name(result));
    httpd_resp_set_type(request,"application/json");
    httpd_resp_sendstr(request,"{\"ok\":true}");
    if(restart) eye_restart_later();
    return ESP_OK;
}
static esp_err_t page_handler(httpd_req_t *request) {
    if(strcmp(request->uri,"/")!=0) {
        httpd_resp_set_status(request,"302 Found");
        httpd_resp_set_hdr(request,"Location","http://192.168.4.1/");
        return httpd_resp_sendstr(request,"Open the configuration portal");
    }
    if(!authenticate(request)) return ESP_OK;
    httpd_resp_set_type(request,"text/html");
    httpd_resp_set_hdr(request,"Cache-Control","no-store");
    httpd_resp_set_hdr(request,"X-Frame-Options","DENY");
    httpd_resp_set_hdr(request,"X-Content-Type-Options","nosniff");
    return httpd_resp_sendstr(request,page_start);
}
static esp_err_t upload_handler(httpd_req_t *request) {
    if(!authenticate(request) || !mutation_allowed(request)) return ESP_OK;
    return eye_ota_upload(request);
}
esp_err_t web_start() {
    char credentials[64];
    snprintf(credentials,sizeof(credentials),"admin:%s",eye_wifi_password());
    size_t encoded=0;
    memcpy(authorization,"Basic ",6);
    if(mbedtls_base64_encode(reinterpret_cast<unsigned char *>(authorization+6),sizeof(authorization)-7,&encoded,
        reinterpret_cast<const unsigned char *>(credentials),strlen(credentials))!=0) return ESP_FAIL;
    authorization[encoded+6]=0;
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.stack_size=8192; config.max_uri_handlers=8;
    config.uri_match_fn=httpd_uri_match_wildcard;
    config.recv_wait_timeout=10; config.lru_purge_enable=true;
    httpd_handle_t server=nullptr;
    esp_err_t result=httpd_start(&server,&config);
    if(result!=ESP_OK) return result;
    const httpd_uri_t routes[]={
        {"/api/status",HTTP_GET,status_handler,nullptr},
        {"/api/control",HTTP_POST,command_handler,nullptr},
        {"/api/calibration",HTTP_POST,command_handler,nullptr},
        {"/api/wifi",HTTP_POST,command_handler,nullptr},
        {"/api/ota",HTTP_POST,upload_handler,nullptr},
        {"/*",HTTP_GET,page_handler,nullptr}
    };
    for(const auto &route:routes) {
        result=httpd_register_uri_handler(server,&route);
        if(result!=ESP_OK) { httpd_stop(server); return result; }
    }
    return ESP_OK;
}
