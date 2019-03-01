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

#include "sdkconfig.h"

#include "esp_audio.h"
#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "periph_console.h"

#include "aligenie_sdk.h"
#include "TopRequest.h"
#include "audio_mem.h"
#include "board.h"

#define AG_DEFAULT_WIFISSID        "GULULU-2.4"
#define AG_DEFAULT_WIFIPWD         "Bowhead311"


#define BUTTON_AI           GPIO_REC    //GPIO_NUM_36
#define BUTTON_AI_SEL       GPIO_SEL_REC

#define BUTTON_MODE         GPIO_MODE   //GPIO_NUM_39
#define BUTTON_MODE_SEL     GPIO_SEL_MODE

#define AG_WIFI_CONNECTED_BIT       BIT0
#define AG_SNTP_FINISH_BIT          BIT1
#define AG_DEVICE_BIND_FINISH_BIT   BIT2


#define WIFI_INFO_HIGH_SSID     "WiFiHighSsid"
#define WIFI_INFO_HIGH_PWD      "WiFiHighPwd"

#define WIFI_INFO_MID_SSID      "WiFiMidSsid"
#define WIFI_INFO_MID_PWD       "WiFiMidPwd"

#define WIFI_INFO_LOW_SSID      "WiFiLowSsid"
#define WIFI_INFO_LOW_PWD       "WiFiLowPwd"

#define BOOT_PARTITION_FACTORY  "factory"
#define BOOT_PARTITION_OTA      "ota"
#define BOOT_PARTITION_BLE      "ble"

typedef struct wifi_info {
    uint8_t ssid[32];      /**< SSID of target AP*/
    uint8_t password[32];  /**< password of target AP*/
} wifi_info_t;

extern EventGroupHandle_t system_event_group;

esp_err_t agButtonInit();
void agSNTPInit();
esp_err_t agPeriphEventHandle(audio_event_iface_msg_t *event, void *context);
esp_err_t agSDCardInit();
void agConsoleInit();
bool agSysEventGroupWait(EventBits_t uxBitsToWaitFor, TickType_t wait_time);
void sysEventGroupInit();
esp_err_t agWifiInit(void);
esp_err_t agPeriphInit();
BaseType_t sntpClientInit();
BaseType_t agSDKInit();
esp_err_t res_flash_read(size_t src_addr, void *buf, size_t byte_wanted);
esp_err_t res_flash_url_search(const char *url, size_t *offset, size_t *len);

