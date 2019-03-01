#include "gll_env.h"

#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "audio_event_iface.h"

#include "esp_peripherals.h"
#include "periph_spiffs.h"
#include "periph_sdcard.h"
#include "periph_wifi.h"


#include "aligenie_sdk.h"
#include "sysinit.h"

// #include "window_main.h"
// #include "msgq.h"

// #include "sys_ex.h"

static const char* TAG ="gll_env";

static audio_event_iface_handle_t listener;

static void gll_periph_event_handle(void *context) {
    audio_event_iface_handle_t listener = (audio_event_iface_handle_t)context;

    while(true) {

        audio_event_iface_msg_t event = {0};

        esp_err_t err = audio_event_iface_listen(listener, &event, portMAX_DELAY);

        if(err == ESP_FAIL) {
            continue;
        }

        switch (event.source_type) {
            case PERIPH_ID_BUTTON:
                ESP_LOGI(TAG, "BUTTON[%d], event.event_id=%d", (int)event.data, event.cmd);

                gpio_num_t gpio_num = (gpio_num_t)event.data;
                periph_button_event_id_t event_id = (periph_button_event_id_t)event.cmd;

                if(gpio_num == BUTTON_AI) {
                    if(event_id == PERIPH_BUTTON_PRESSED) {
                        ag_sdk_notify_keyevent(AG_KEYEVENT_AI_START);
                    } 
                    else if(event_id == PERIPH_BUTTON_RELEASE) {
                        ag_sdk_notify_keyevent(AG_KEYEVENT_AI_STOP);

                    }
                    else if(event_id == PERIPH_BUTTON_LONG_RELEASE) {
                        ag_sdk_notify_keyevent(AG_KEYEVENT_AI_STOP);
                    }
                }
                // else if(gpio_num == BUTTON_MODE) {
                //     if(event_id == PERIPH_BUTTON_PRESSED) {
                //         ag_sdk_notify_keyevent(AG_KEYEVENT_ADD_REMOVE_FAVORITE);
                //     } 
                //     else if(event_id == PERIPH_BUTTON_LONG_PRESSED) {
                //         ag_sdk_notify_keyevent(AG_KEYEVENT_ADD_REMOVE_FAVORITE);
                //     }
                // }
                break;
            case PERIPH_ID_TOUCH:
                ESP_LOGI(TAG, "TOUCH[%d], event.event_id=%d", (int)event.data, event.cmd);
                break;
            case PERIPH_ID_SDCARD:
                ESP_LOGI(TAG, "SDCARD status, event->event_id=%d", event.cmd);
                break;
            case PERIPH_ID_WIFI:
                ESP_LOGI(TAG, "WIFI, event->event_id=%d", event.cmd);
                break;
            case PERIPH_ID_FLASH:
                ESP_LOGI(TAG, "FLASH, event->event_id=%d", event.cmd);
                break;
            case PERIPH_ID_AUXIN:
                break;
            case PERIPH_ID_ADC:
                break;
            case PERIPH_ID_CONSOLE:
                ESP_LOGI(TAG, "CONSOLE, event->event_id=%d", event.cmd);
                break;
            case PERIPH_ID_BLUETOOTH:
                break;
            case PERIPH_ID_LED:
                break;
        }
    }

    vTaskDelete(NULL);
}

static void gll_wifi_init() {

    tcpip_adapter_init();

    periph_wifi_cfg_t wifi_cfg = {
        .disable_auto_reconnect = false,
        .reconnect_timeout_ms = 5000,
        .ssid = "GULULU-2.4",
        .password = "Bowhead311" 
    };

    esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    ESP_ERROR_CHECK(esp_periph_start(wifi_handle));

    periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

}

static void gll_spiffs_init() {
    periph_spiffs_cfg_t spiffs_cfg = {
        .root = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_periph_handle_t spiffs_handle = periph_spiffs_init(&spiffs_cfg);

    ESP_ERROR_CHECK(esp_periph_start(spiffs_handle));

    // Wait until spiffs was mounted
    while (!periph_spiffs_is_mounted(spiffs_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void gll_sdcard_init() {

    periph_sdcard_cfg_t sdcard_cfg = {
        .root = "/sdcard",
        .card_detect_pin = SD_CARD_INTR_GPIO, // GPIO_NUM_34
    };
    esp_periph_handle_t sdcard_handle = periph_sdcard_init(&sdcard_cfg);

    ESP_ERROR_CHECK(esp_periph_start(sdcard_handle));

    // Wait until sdcard was mounted
    while (!periph_sdcard_is_mounted(sdcard_handle)) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void gll_button_init() {

    periph_button_cfg_t button_cfg = {
        .gpio_mask = BUTTON_AI_SEL | BUTTON_MODE_SEL,
        .long_press_time_ms = 3*1000,
    };
    esp_periph_handle_t button_handle = periph_button_init(&button_cfg);
    
    ESP_ERROR_CHECK(esp_periph_start(button_handle));
}

static void gll_periph_init() {

    esp_periph_config_t periph_cfg = {0};
    ESP_ERROR_CHECK(esp_periph_init(&periph_cfg));

    audio_event_iface_cfg_t event_iface_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    listener = audio_event_iface_init(&event_iface_cfg);
    audio_event_iface_set_listener(esp_periph_get_event_iface(), listener);
    BaseType_t ret = xTaskCreate(gll_periph_event_handle, "gll_periph_handler",
                                             4*1024, listener, 10, NULL);
    assert(ret == pdPASS);

    gll_spiffs_init();
    gll_sdcard_init();
    gll_button_init();
    gll_wifi_init();
}

// static void gll_env_initialize_msgqs() {
//     ESP_LOGI(TAG, "gll_env_initialize_msgqs");

//     // mount_window("/window");
//     mount_msgq("/msgq");    
// }

static void gll_env_initialize_nvs() {
    ESP_LOGI(TAG, "gll_env_initialize_nvs");
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void gll_env_init() {

    ESP_LOGI(TAG, "start...");
    
    gll_env_initialize_nvs();
    
    // gll_env_initialize_msgqs();

    gll_periph_init();

    ESP_LOGI(TAG, "end");
}
