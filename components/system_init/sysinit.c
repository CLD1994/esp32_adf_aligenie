/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>



#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "sdmmc_cmd.h"
#include "lwip/err.h"
#include "apps/sntp/sntp.h"

#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_system.h"


#include "sdkconfig.h"

#include "esp_audio.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "periph_console.h"

#include "aligenie_sdk.h"
#include "aligenie_os.h"
#include "TopRequest.h"
#include "audio_mem.h"
#include "board.h"
#include "sysinit.h"
#include "freertos/timers.h"


static const char *TAG = "sysinit";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
EventGroupHandle_t system_event_group;

bool agSysEventGroupWait(EventBits_t uxBitsToWaitFor, TickType_t wait_time)
{
    EventBits_t uxBits = xEventGroupWaitBits(system_event_group, uxBitsToWaitFor, false, false, wait_time);
    if((uxBits&uxBitsToWaitFor) != uxBitsToWaitFor) {
        ESP_LOGE(TAG, "Time out for BIT[%d] event grout wait[%d]", uxBitsToWaitFor, uxBits);
        return false;
    }
    return true;
}

static void agSDKInitTask(void *pvParameters)
{
#if 1
    const char* appkey  = "25042116";
    const char* userid  = "12345678";
    const char* appsecret = "036bc0f8619849c8671d1c971838f4ba";
    const char* schema  = "638A1B4AD2E2F330C4ABAF94EBF64CC2";
#endif

#if 0
    const char* appkey  = "25058538";
    const char* userid  = "12348765";
    const char* appsecret = "36ee33d2b73d60e5c2318c23711e3356";
    const char* schema  = "50ECAB1B4079DBAE986176985994E900";
#endif


    // agSysEventGroupWait(AG_WIFI_CONNECTED_BIT, portMAX_DELAY);
    // agSysEventGroupWait(AG_SNTP_FINISH_BIT, portMAX_DELAY);

    char mac_addr[16];
    ag_os_get_mac_address(mac_addr);
    
    char mac_addr_raw[32];
    ag_os_get_mac_address_raw(mac_addr_raw);
    
    userid = mac_addr_raw;
    userid = "12345678";
    char *authcode = NULL;
    
    authcode = getAuthCodeFromTop(appkey, userid, appsecret, schema);

    ESP_LOGI(TAG, "authcode = %s", authcode);

    if(authcode != NULL) 
    {
        ag_sdk_set_register_info(mac_addr, userid, authcode, 0);
        ag_sdk_init();
        vPortFree(authcode);
    }

AUDIO_MEM_SHOW(TAG);

    vTaskDelete(NULL);
}

void sysEventGroupInit()
{
    system_event_group = xEventGroupCreate();

    xEventGroupClearBits(system_event_group, AG_WIFI_CONNECTED_BIT);
    xEventGroupClearBits(system_event_group, AG_SNTP_FINISH_BIT);
    xEventGroupClearBits(system_event_group, AG_DEVICE_BIND_FINISH_BIT);
}

#if 0
static TimerHandle_t g_button_check_timer = NULL;

void buttonCheckInterval(TimerHandle_t xTimer)
{
    static unsigned char check_cnt = 0;
    gpio_num_t gpio_num = (gpio_num_t)pvTimerGetTimerID(xTimer); 
    char button_level_new;
    static char button_level_old;
    bool button_level_changed = false;

    button_level_old = button_level_new = (char)gpio_get_level(gpio_num);

    check_cnt++;

    ESP_LOGW(TAG, "GPIO[%d] total check count[%d] with value[%d] ....", gpio_num, check_cnt, button_level_new);

    if(button_level_new == BUTTON_UP) 
    {
        button_level_changed = true;
    }
    else
    {
        if(button_level_new != button_level_old) {
            button_level_changed  = true;
        }
        else if(check_cnt == BUTTON_HOLDON_CNT)
        {
            ESP_LOGI(TAG, "GPIO[%d] kept press and system enter upgrade mode ....", gpio_num);

            if(g_button_check_timer != NULL) 
            {
                xTimerStop(g_button_check_timer, 0);
                xTimerDelete(g_button_check_timer, 0);
                g_button_check_timer = NULL;
            }
    
            if(agSysSetBootPartition(BOOT_PARTITION_BLE) < 0)
            {
                ESP_LOGE(TAG, "Set boot up from BT partition failed\n");
            }
    
            esp_restart();
        }
    }
    
    if(check_cnt == BUTTON_HOLDON_CNT
        || button_level_changed == true) 
    {
        ESP_LOGE(TAG, "stop timer with button new level[%d], old level[%d], check cnt[%d]", 
                    button_level_new, button_level_old, check_cnt);
        if(g_button_check_timer != NULL) 
        {
            xTimerStop(g_button_check_timer, 0);
            xTimerDelete(g_button_check_timer, 0);
            g_button_check_timer = NULL;
        }
        check_cnt = 0;
        button_level_old = 0;
    }
    else
    {
        button_level_old = button_level_new;
    }
}

void buttonPressCheck(gpio_num_t gpio_num)
{
    if(g_button_check_timer != NULL) {
        xTimerStop(g_button_check_timer, 0);
        xTimerDelete(&g_button_check_timer, 0);
        g_button_check_timer = NULL;
    }

    g_button_check_timer = xTimerCreate("BootModel", 
                              pdMS_TO_TICKS(1000), 
                              true,
                              (void*)gpio_num,
                              buttonCheckInterval);
    
    xTimerStart(g_button_check_timer, 0);
}
#endif

BaseType_t agSDKInit()
{
    return xTaskCreate(agSDKInitTask, "agsdkinit", 1024*4, NULL, 5, NULL);
}

