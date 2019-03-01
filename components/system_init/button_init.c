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

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "periph_button.h"
#include "sysinit.h"
#include "esp_system.h"


static const char *TAG = "button";

#if CONFIG_PLATFORM_CODEC_ES7148

static void adc_check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Two Point: Supported");
    } else {
        ESP_LOGE(TAG, "eFuse Two Point: NOT supported");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Vref: Supported");
    } else {
        ESP_LOGE(TAG, "eFuse Vref: NOT supported");
    }
}

int es7148_get_voice_volume(int *);
int es7148_set_voice_volume(int);

void agButtonTask(void * pararmeter)
{
    while (1) 
    {
        volatile uint32_t adc_reading = 0;
        int volume = 0;

        adc_reading = adc1_get_raw(ADC1_CHANNEL_0);

        if(adc_reading>1200 && adc_reading<1500)
        {
            // audioManagerGetStreamVolume(&volume);
            // volume = volume - 5;
            // audioManagerSetStreamVolume(volume);
        }
        else if(adc_reading>1500 && adc_reading<2000)
        {
            // audioManagerGetStreamVolume(&volume);
            // volume = volume + 5;
            // audioManagerSetStreamVolume(volume);
        }
        
        if(adc_reading>500 && adc_reading<1200) 
        {
            ESP_LOGW(TAG, "Device enter WiFi configuration mode ........");

            // AudioTrackInfo msg;
            // msg.data = "/res/network_config.mp3";
            // msg.cmd = AEL_MSG_CMD_START;
            // msg.type= kAudioStreamPrompt;
            // audioPlayerPrompt(&msg);
            // audioPlayerWaitForFinish(msg.player_handle, portMAX_DELAY);

            // agSysSetBootPartition("ble");
            // esp_restart();
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }

}

esp_err_t agButtonInit()
{
    esp_err_t err;
    adc_check_efuse();

    //Configure ADC
    err = adc1_config_width(ADC_WIDTH_BIT_12);
    err |= adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);

    xTaskCreate(agButtonTask, "button", 1024*4, NULL, 3, NULL);
    return err;
}

#elif CONFIG_PLATFORM_CODEC_ES8388

esp_err_t agButtonInit(void)
{
    esp_err_t ret = ESP_FAIL;

    // Initialize button peripheral
    periph_button_cfg_t button_cfg = {
        .gpio_mask = BUTTON_AI_SEL | BUTTON_MODE_SEL,
        .long_press_time_ms = 3*1000,
    };
    esp_periph_handle_t button_handle = periph_button_init(&button_cfg);

    return esp_periph_start(button_handle);
}

#endif

