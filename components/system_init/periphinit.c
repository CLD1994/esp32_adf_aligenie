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

#include "esp_log.h"


#include "sdkconfig.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"
#include "periph_button.h"
#include "periph_console.h"

#include "audio_mem.h"
#include "board.h"
#include "sysinit.h"
#include "aligenie_sdk.h"


static const char *TAG = "periph";

/**************************************************************************/
esp_err_t agPeriphEventHandle(audio_event_iface_msg_t *event, void *context)
{
    switch ((int)event->source_type) {
        case PERIPH_ID_BUTTON:
            ESP_LOGI(TAG, "BUTTON[%d], event->event_id=%d", (int)event->data, event->cmd);

            if((int)event->data == BUTTON_AI) 
            {
                if(event->cmd == 0x01) 
                {
                    ag_sdk_notify_keyevent(AG_KEYEVENT_AI_START);
                } 
                else if(event->cmd == 0x02) 
                {
                    ag_sdk_notify_keyevent(AG_KEYEVENT_AI_STOP);
                }
            }
            else if((int)event->data == BUTTON_MODE) 
            {
                if(event->cmd == 0x01) 
                {
                    ag_sdk_notify_keyevent(AG_KEYEVENT_ADD_REMOVE_FAVORITE);
                } 
                else if(event->cmd == 0x03) 
                {
                    ag_sdk_notify_keyevent(AG_KEYEVENT_ADD_REMOVE_FAVORITE);
                }
            }
            
            break;
        case PERIPH_ID_SDCARD:
            ESP_LOGI(TAG, "SDCARD status, event->event_id=%d", event->cmd);
            break;
        case PERIPH_ID_TOUCH:
            ESP_LOGI(TAG, "TOUCH[%d], event->event_id=%d", (int)event->data, event->cmd);
            break;
        case PERIPH_ID_WIFI:
            ESP_LOGI(TAG, "WIFI, event->event_id=%d", event->cmd);
            break;
        case PERIPH_ID_CONSOLE:
            ESP_LOGI(TAG, "CONSOLE, event->event_id=%d", event->cmd);
            break;
    }
    return ESP_OK;
}

esp_err_t agSDCardInit()
{
    esp_err_t ret = ESP_FAIL;

    ESP_LOGI(TAG, "Start SdCard");
    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, // GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);
    return esp_periph_start(sdcard_handle);

}


esp_err_t agPeriphInit()
{
    esp_periph_config_t periph_cfg = {
        .event_handle = agPeriphEventHandle,
        .user_context = NULL,
    };
    
    return esp_periph_init(&periph_cfg);
}

