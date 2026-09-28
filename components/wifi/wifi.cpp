#include "eye_wifi.hpp"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <cstdio>
static char ap_name[32]={};
static esp_timer_handle_t reconnect_timer=nullptr;
static int dns_socket=-1;
struct Credentials { char ssid[33]; char password[65]; };
static void reconnect(void *) { esp_wifi_connect(); }
static void wifi_event(void *,esp_event_base_t base,int32_t id,void *) {
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_START) esp_wifi_connect();
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED) {
        esp_timer_stop(reconnect_timer);
        esp_timer_start_once(reconnect_timer,5000000);
    }
    if(base==IP_EVENT && id==IP_EVENT_STA_GOT_IP) ESP_LOGI("wifi","STA connected; recovery AP remains available");
}
static void dns_task(void *) {
    int server=dns_socket;
    uint8_t packet[512];
    while(true) {
        sockaddr_in peer={}; socklen_t peer_size=sizeof(peer);
        int length=recvfrom(server,packet,sizeof(packet)-16,0,reinterpret_cast<sockaddr *>(&peer),&peer_size);
        if(length<17 || (packet[2]&0x80) || packet[4]!=0 || packet[5]!=1) continue;
        int cursor=12;
        while(cursor<length && packet[cursor]!=0) {
            int label=packet[cursor];
            if(label>63 || cursor+label+1>=length) { cursor=length; break; }
            cursor+=label+1;
        }
        if(cursor+5>length) continue;
        bool ipv4=packet[cursor+1]==0 && packet[cursor+2]==1 && packet[cursor+3]==0 && packet[cursor+4]==1;
        int end=cursor+5;
        packet[2]=0x81; packet[3]=0x80; packet[6]=0; packet[7]=ipv4 ? 1 : 0;
        packet[8]=packet[9]=packet[10]=packet[11]=0;
        if(ipv4) {
            const uint8_t answer[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4,192,168,4,1};
            memcpy(packet+end,answer,sizeof(answer)); end+=sizeof(answer);
        }
        sendto(server,packet,end,0,reinterpret_cast<sockaddr *>(&peer),peer_size);
    }
}
esp_err_t eye_wifi_save(const char *ssid,const char *password) {
    size_t ssid_length=strlen(ssid), password_length=strlen(password);
    if(ssid_length<1 || ssid_length>32 || password_length>63 || (password_length>0 && password_length<8)) return ESP_ERR_INVALID_ARG;
    Credentials credentials={};
    memcpy(credentials.ssid,ssid,ssid_length);
    memcpy(credentials.password,password,password_length);
    nvs_handle_t handle;
    esp_err_t result=nvs_open("network",NVS_READWRITE,&handle);
    if(result!=ESP_OK) return result;
    result=nvs_set_blob(handle,"station",&credentials,sizeof(credentials));
    if(result==ESP_OK) result=nvs_commit(handle);
    nvs_close(handle);
    return result;
}
const char *eye_wifi_ap_name() { return ap_name; }
esp_err_t eye_wifi_start() {
    nvs_handle_t handle;
    esp_err_t result=nvs_open("network",NVS_READWRITE,&handle);
    if(result!=ESP_OK) return result;
    Credentials credentials={};
    size_t size=sizeof(credentials);
    esp_err_t station_result=nvs_get_blob(handle,"station",&credentials,&size);
    nvs_close(handle);
    if(result!=ESP_OK) return result;
    bool station=station_result==ESP_OK && size==sizeof(credentials) && credentials.ssid[0] &&
        memchr(credentials.ssid,0,sizeof(credentials.ssid)) && memchr(credentials.password,0,sizeof(credentials.password));
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    if(!esp_netif_create_default_wifi_ap() || !esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
    wifi_init_config_t init=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    esp_timer_create_args_t timer_args={};
    timer_args.callback=reconnect; timer_args.name="wifi_retry";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args,&reconnect_timer));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,nullptr));
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac,ESP_MAC_WIFI_SOFTAP));
    snprintf(ap_name,sizeof(ap_name),"AnimatronicEye-%02X%02X%02X",mac[3],mac[4],mac[5]);
    wifi_config_t access_point={};
    memcpy(access_point.ap.ssid,ap_name,strlen(ap_name));
    access_point.ap.ssid_len=strlen(ap_name);
    access_point.ap.channel=1; access_point.ap.max_connection=4;
    access_point.ap.authmode=WIFI_AUTH_OPEN;
    access_point.ap.ssid_hidden=0;
    ESP_ERROR_CHECK(esp_wifi_set_mode(station ? WIFI_MODE_APSTA : WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP,&access_point));
    if(station) {
        wifi_config_t station_config={};
        memcpy(station_config.sta.ssid,credentials.ssid,strlen(credentials.ssid));
        memcpy(station_config.sta.password,credentials.password,strlen(credentials.password));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&station_config));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    dns_socket=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(dns_socket<0) return ESP_FAIL;
    sockaddr_in local={};
    local.sin_family=AF_INET; local.sin_port=htons(53); local.sin_addr.s_addr=htonl(0xc0a80401);
    if(bind(dns_socket,reinterpret_cast<sockaddr *>(&local),sizeof(local))<0) { close(dns_socket); return ESP_FAIL; }
    ESP_LOGI("wifi","AP: %s | Open Wi-Fi | Portal: http://192.168.4.1 | No login",ap_name);
    return xTaskCreate(dns_task,"captive_dns",3072,nullptr,3,nullptr)==pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
