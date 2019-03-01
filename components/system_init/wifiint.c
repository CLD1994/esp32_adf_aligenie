/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_blufi_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#include "sysinit.h"
#include "aligenie_os.h"
#include "aligenie_sdk.h"


#define AUDIO_ERR_CHECK(TAG, a, action) if ((a)) {                                       \
        ESP_LOGE(TAG,"%s:%d (%s): Error[%d]", __FILENAME__, __LINE__, __FUNCTION__, a);       \
        action;                                                                   \
        }
        
const char *TAG = "wifiinit";

static wifi_config_t sta_config;
static wifi_config_t ap_config;

/* store the station info for send back to phone */
static bool gl_sta_connected = false;
static uint8_t gl_sta_bssid[6];
static uint8_t gl_sta_ssid[32];
static int gl_sta_ssid_len;

/* connect infor*/
static uint8_t server_if;
static uint16_t conn_id;

static esp_err_t wifi_sta_scan()
{
    esp_err_t ret;

    wifi_scan_config_t scanConf = {0};
    scanConf.scan_time.active.min = 300;
    scanConf.scan_time.active.max = 500;
    
    ret = esp_wifi_scan_start(&scanConf, false);
    if(ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start wifi scan with err[%s]", esp_err_to_name(ret));
    }

    return ret;
}

static esp_err_t wifi_sta_connect(wifi_ap_record_t *ap_list, uint16_t apCount)
{
    esp_err_t ret;
    wifi_config_t wifi_config = { 0 };
    char ssid_info_high[33] = { 0 };
    char ssid_info_mid[33] = { 0 };
    char ssid_info_low[33] = { 0 };
    char ssid_info_pwd[64] = { 0 };
    int index = 0;
    char info_flag = 0x0;
    
#define  INFO_HIGH  0x4
#define  INFO_MID   0x2
#define  INFO_LOW   0x1

    ag_os_flash_read(WIFI_INFO_HIGH_SSID, ssid_info_high, sizeof(ssid_info_high));
    ag_os_flash_read(WIFI_INFO_MID_SSID, ssid_info_mid, sizeof(ssid_info_mid));
    ag_os_flash_read(WIFI_INFO_LOW_SSID, ssid_info_low, sizeof(ssid_info_low));

    for(index=0; index<apCount; index++)
    {
        if(strcmp((char *)ap_list[index].ssid, ssid_info_high) == 0) {
            info_flag = INFO_HIGH;
            break;
        }
        else if(strcmp((char *)ap_list[index].ssid, ssid_info_mid) == 0) {
            info_flag = INFO_MID;
            break;
        }
        else if(strcmp((char *)ap_list[index].ssid, ssid_info_low) == 0) {
            info_flag = INFO_LOW;
            break;
        }
    }

    if(index == apCount) 
    {
        ESP_LOGE(TAG, "Faile to find available AP, give up ....");
        
#if CONFIG_BT_ENABLED
 
#else
        // AudioTrackInfo msg;
        // msg.data = "/res/network_not_connected.mp3";
        // msg.cmd = AM_MSG_CMD_START;
        // msg.type= kAudioStreamPrompt;
        // audioPlayerPrompt(&msg);
        // audioPlayerWaitForFinish(msg.player_handle, portMAX_DELAY);
#endif
        return ESP_FAIL;
    }      

    ESP_LOGI(TAG, "AP[%s] available, try to connect to it....", ap_list[index].ssid);

    switch(info_flag){
        case INFO_HIGH:
            strcpy((char*)wifi_config.sta.ssid, ssid_info_high);
            ag_os_flash_read(WIFI_INFO_HIGH_PWD, ssid_info_pwd, sizeof(ssid_info_pwd)); 
            break;

        case INFO_MID:
            strcpy((char*)wifi_config.sta.ssid, ssid_info_mid);
            ag_os_flash_read(WIFI_INFO_MID_PWD, ssid_info_pwd, sizeof(ssid_info_pwd)); 
            break;

        case INFO_LOW:
            strcpy((char*)wifi_config.sta.ssid, ssid_info_low);
            ag_os_flash_read(WIFI_INFO_LOW_PWD, ssid_info_pwd, sizeof(ssid_info_pwd)); 
            break;
    }
    strcpy((char*)wifi_config.sta.password, ssid_info_pwd); 
    
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
    
    return ret;
}

static esp_err_t wifi_net_event_handler(void *ctx, system_event_t *event)
{
    wifi_mode_t mode;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            
#ifdef CONFIG_PLATFORM_BLE_MODE
            esp_wifi_connect();
#endif

#ifdef CONFIG_PLATFORM_NORMAL_MODE
            wifi_sta_scan();
#endif
            break;
        case SYSTEM_EVENT_STA_GOT_IP: {
            xEventGroupSetBits(system_event_group, AG_WIFI_CONNECTED_BIT);
               
#if CONFIG_BT_ENABLED
            esp_wifi_get_mode(&mode);
            esp_blufi_extra_info_t info;
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
            esp_restart();
#else
#if 0
            AudioTrackInfo msg;
            msg.data = "/res/network_connected.mp3";
            msg.cmd = AM_MSG_CMD_START;
            msg.type= kAudioStreamPrompt;
            audioPlayerPrompt(&msg);
            audioPlayerMsgSend(&msg);
            audioPlayerWaitForFinish(msg.player_handle, portMAX_DELAY);
#endif
#endif

            break;
        }
        case SYSTEM_EVENT_STA_CONNECTED:
            gl_sta_connected = true;
            memcpy(gl_sta_bssid, event->event_info.connected.bssid, 6);
            memcpy(gl_sta_ssid, event->event_info.connected.ssid, event->event_info.connected.ssid_len);
            gl_sta_ssid_len = event->event_info.connected.ssid_len;

            ag_sdk_notify_network_status_change(AG_NETWORK_CONNECTED);
            
            break; 
        case SYSTEM_EVENT_STA_DISCONNECTED:
            /* This is a workaround as ESP32 WiFi libs don't currently
               auto-reassociate. */

            ag_sdk_notify_network_status_change(AG_NETWORK_DISCONNECT);
            
            gl_sta_connected = false;
            memset(gl_sta_ssid, 0, 32);
            memset(gl_sta_bssid, 0, 6);
            gl_sta_ssid_len = 0;
            esp_wifi_connect();
            xEventGroupClearBits(system_event_group, AG_WIFI_CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_AP_START:
            esp_wifi_get_mode(&mode);
            
#if CONFIG_BT_ENABLED
            /* TODO: get config or information of softap, then set to report extra_info */
            if (gl_sta_connected) {  
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, NULL);
            } else {
                esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_FAIL, 0, NULL);
            }
#endif
            break;
        case SYSTEM_EVENT_SCAN_DONE: {
            uint16_t apCount = 0;
            esp_wifi_scan_get_ap_num(&apCount);
            if (apCount == 0) {
                ESP_LOGI(TAG, "Nothing AP found");
                break;
            }
            wifi_ap_record_t *ap_list = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * apCount);
            if (!ap_list) {
                ESP_LOGE(TAG, "malloc error, ap_list is NULL");
                break;
            }
            ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&apCount, ap_list));

            for (int i = 0; i < apCount; ++i) {
                ESP_LOGI(TAG, "Scaned wifi with ssid[%s], rssi[%d]", ap_list[i].ssid, ap_list[i].rssi);
            }
            esp_wifi_scan_stop();
            wifi_sta_connect(ap_list, apCount);
            
 #if CONFIG_BT_ENABLED
            esp_blufi_ap_record_t * blufi_ap_list = (esp_blufi_ap_record_t *)malloc(apCount * sizeof(esp_blufi_ap_record_t));
            if (!blufi_ap_list) {
                if (ap_list) {
                    free(ap_list);
                }
                ESP_LOGE(TAG, "malloc error, blufi_ap_list is NULL");
                break;
            }
            for (int i = 0; i < apCount; ++i)
            {
                blufi_ap_list[i].rssi = ap_list[i].rssi;
                memcpy(blufi_ap_list[i].ssid, ap_list[i].ssid, sizeof(ap_list[i].ssid));
                ESP_LOGI(TAG, "Scaned wifi with ssid[%s], rssi[%d]", ap_list[i].ssid, ap_list[i].rssi);
            }
            esp_blufi_send_wifi_list(apCount, blufi_ap_list);
            free(blufi_ap_list);
#endif
            free(ap_list);
            break;
        }
        default:
            break;
    }
    return ESP_OK;
}

#if 0
esp_err_t agWifiInit(void)
{
    esp_err_t err;

    err = esp_event_loop_init(wifi_net_event_handler, NULL);
    AUDIO_ERR_CHECK(TAG, err, goto exit);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    AUDIO_ERR_CHECK(TAG, err, goto exit);

    err = esp_wifi_set_mode(WIFI_MODE_STA);
    AUDIO_ERR_CHECK(TAG, err, goto exit);

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    AUDIO_ERR_CHECK(TAG, err, goto exit);
    
    err = esp_wifi_start();
    AUDIO_ERR_CHECK(TAG, err, goto exit);
    
exit:
    return err;
}

#else
esp_err_t agWifiInit(void)
{
    esp_err_t err;

    err = esp_event_loop_init(wifi_net_event_handler, NULL);
    AUDIO_ERR_CHECK(TAG, err, goto exit);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    AUDIO_ERR_CHECK(TAG, err, goto exit);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = AG_DEFAULT_WIFISSID,
            .password = AG_DEFAULT_WIFIPWD,
        },
    };
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    AUDIO_ERR_CHECK(TAG, err, goto exit);
    
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    AUDIO_ERR_CHECK(TAG, err, goto exit);
      
    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    AUDIO_ERR_CHECK(TAG, err, goto exit);
    
    err = esp_wifi_start();
    AUDIO_ERR_CHECK(TAG, err, goto exit);
   
    err = esp_wifi_connect();
    AUDIO_ERR_CHECK(TAG, err, goto exit);

exit:
    return err;
}
#endif

