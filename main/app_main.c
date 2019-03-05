#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "aligenie_sdk.h"
#include "gll_env.h"


#include "audio_mem.h"
#include "sysinit.h"
#include "bluficfg.h"

#include "audio_manager.h"

static const char *TAG = "app_normal";

void app_main()
{

    gll_env_init();

    audio_manager_cfg_t cfg = {0};
    audio_manager_init(&cfg);

    // audio_manager_start(AUDIO_STREAM_URL, "http://tmjl128.alicdn.com/626/2110216626/2104164259/1806591899_1540954803026.mp3?auth_key=1552359600-0-0-65ce09359938ae1e06f01d5b367569c4");

    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    // audio_manager_pause(AUDIO_STREAM_URL);

    // vTaskDelay(4000 / portTICK_PERIOD_MS);

    // audio_manager_start(AUDIO_STREAM_TTS, "http://tmjl128.alicdn.com/691/123691/1104925305/1773350897_15563968_l.mp3?auth_key=1552359600-0-0-6bde88ec6a2de9b07e2b180c1ae8a3b3");

    // vTaskDelay(10000 / portTICK_PERIOD_MS);

    // audio_manager_stop(AUDIO_STREAM_TTS);

    // audio_manager_resume(AUDIO_STREAM_URL);

   	// vTaskDelay(10000 / portTICK_PERIOD_MS);

    // audio_manager_pause(AUDIO_STREAM_URL);

    // vTaskDelay(4000 / portTICK_PERIOD_MS);

    // audio_manager_resume(AUDIO_STREAM_URL);

    sntpClientInit();

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    agSDKInit();
}

