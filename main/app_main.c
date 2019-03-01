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

    sntpClientInit();

    vTaskDelay(4000 / portTICK_PERIOD_MS);

    agSDKInit();
}

