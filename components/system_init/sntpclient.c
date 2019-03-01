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
#include "apps/sntp/sntp.h"
#include "esp_log.h"
#include "sysinit.h"

static  const char *TAG = "sntpclient";

static void agSNTPCfg(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp1.aliyun.com");
    sntp_setservername(1, "cn.ntp.org.cn");
    sntp_setservername(2, "time2.google.com");
    sntp_setservername(3, "cn.pool.ntp.org");
    sntp_init();
}

static int agSNTPGet(void)
{
    // agSysEventGroupWait(AG_WIFI_CONNECTED_BIT, portMAX_DELAY);

    agSNTPCfg();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const short retry_count = 60;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) 
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    return 0;
}

static void agNetTimeTask(void *pvParameter)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Connecting to WiFi and getting time over NTP.");
        if(agSNTPGet() < 0){
            goto exit;
        }
            
        // update 'now' variable with current time
        time(&now);
    }

    // xEventGroupSetBits(system_event_group, AG_SNTP_FINISH_BIT);
    
    // Set timezone to China Standard Time
    char strftime_buf[64];
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in China is: %s", strftime_buf);

exit:
    vTaskDelete(NULL);
}

/**************************************************************************/

BaseType_t sntpClientInit()
{
    return xTaskCreate(agNetTimeTask, "agnettime", 1024 * 4, NULL, 6, NULL);
}

