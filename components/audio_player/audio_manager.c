#include "audio_manager.h"

#include <string.h>
#include "esp_log.h"

#include "audio_hal.h"
#include "audio_mem.h"

static const char *TAG = "audiomanager";

static audio_manager_handle_t s_audio_manager_handle = NULL;

struct audio_manager {
    audio_hal_handle_t      audio_hal;

    audio_player_handle_t   url_player_handle;
    audio_player_handle_t   tts_player_handle;
    audio_player_handle_t   prompt_player_handle;

    int                     player_active_bits;

    audio_manager_state_callback state_callback;
};

static esp_err_t _audio_player_callback(audio_player_handle_t player_handle, audio_element_state_t status) {
    if(s_audio_manager_handle->state_callback) {
        return s_audio_manager_handle->state_callback(player_handle, status);
    }

    return ESP_OK;
}

static esp_err_t audio_manager_init_aduio_hal(audio_manager_handle_t audio_manager_handle) {
    // Setup audio codec
    ESP_LOGI(TAG, "setup audio codec");

    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_codec_cfg.i2s_iface.samples = AUDIO_HAL_44K_SAMPLES;
    audio_hal_handle_t audio_hal = NULL;

    bool success = (
            (audio_hal = audio_hal_init(&audio_hal_codec_cfg, 0)) &&
            (audio_hal_ctrl_codec(audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_START) == ESP_OK)
        );

    if (success) {
        audio_manager_handle->audio_hal = audio_hal;
        return ESP_OK;
    }else {
        if (audio_hal){
            audio_hal_deinit(audio_hal, 0);
        }
        return ESP_FAIL;
    }
}

esp_err_t audio_manager_init(audio_manager_cfg_t *config) {

    if (s_audio_manager_handle) {
        return ESP_OK;
    }

    s_audio_manager_handle = (audio_manager_handle_t)audio_calloc(1, sizeof(struct audio_manager));

    AUDIO_MEM_CHECK(TAG, s_audio_manager_handle, {
        return ESP_FAIL;
    });

    if(audio_manager_init_aduio_hal(s_audio_manager_handle) != ESP_OK) {
        return ESP_FAIL;
    }

    s_audio_manager_handle->state_callback = config->state_callback;

    audio_player_cfg_t url_player_cfg = {
        .rb_size = 8*1024,
        .callback = _audio_player_callback,
    };

    audio_player_cfg_t tts_player_cfg = {
        .rb_size = 8*1024,
        .callback = _audio_player_callback,
    };
    audio_player_cfg_t prompt_player_cfg = {
        .rb_size = 8*1024,
        .callback = _audio_player_callback,
    };

    bool success = (
            (s_audio_manager_handle->url_player_handle = audio_player_create(&url_player_cfg)) &&
            (s_audio_manager_handle->tts_player_handle = audio_player_create(&tts_player_cfg)) &&
            (s_audio_manager_handle->prompt_player_handle = audio_player_create(&prompt_player_cfg))
        );

    AUDIO_MEM_CHECK(TAG, success, goto failed);

    return ESP_OK;

failed:
    audio_player_destroy(s_audio_manager_handle->url_player_handle);
    audio_player_destroy(s_audio_manager_handle->tts_player_handle);
    audio_player_destroy(s_audio_manager_handle->prompt_player_handle);
    return ESP_FAIL;
}

esp_err_t audio_manager_deinit() {
    audio_player_destroy(s_audio_manager_handle->url_player_handle);
    audio_player_destroy(s_audio_manager_handle->tts_player_handle);
    audio_player_destroy(s_audio_manager_handle->prompt_player_handle);

    audio_hal_ctrl_codec(s_audio_manager_handle->audio_hal, AUDIO_HAL_CODEC_MODE_BOTH, AUDIO_HAL_CTRL_STOP);
    audio_hal_deinit(s_audio_manager_handle->audio_hal, 0);

    return ESP_OK;
}

esp_err_t audio_manager_start(audio_player_type_t type, const char *url) {

    switch(type) {
        case AUDIO_STREAM_URL:
            break;
        case AUDIO_STREAM_TTS:
            
            break;
        case AUDIO_STREAM_PROMPT:
            audio_player_start(s_audio_manager_handle->prompt_player_handle, url);
            break;
        default:
            break;
    }

    return ESP_OK;
}

esp_err_t audio_manager_stop(audio_player_type_t type) {

    switch(type) {
        case AUDIO_STREAM_URL:
            break;
        case AUDIO_STREAM_TTS:
            break;
        case AUDIO_STREAM_PROMPT:
            break;
        default:
            break;
    }

    return ESP_OK;
}

esp_err_t audio_manager_resume(audio_player_type_t type) {
    switch(type) {
        case AUDIO_STREAM_URL:
            break;
        case AUDIO_STREAM_TTS:
            break;
        case AUDIO_STREAM_PROMPT:
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t audio_manager_pause(audio_player_type_t type) {
    switch(type) {
        case AUDIO_STREAM_URL:
            break;
        case AUDIO_STREAM_TTS:
            break;
        case AUDIO_STREAM_PROMPT:
            break;
        default:
            break;
    }
    return ESP_OK;
}

audio_focus_state_t audio_manager_request_focus(audio_player_type_t type) {
    
    audio_focus_state_t focus_state = AUDIO_FOCUS_GAIN;

    // int player_active_bits = audio_manager_handle->player_active_bits;

    // switch(type) {
    //     case AUDIO_STREAM_URL:
    //         //Lowest priority
    //         if ( (player_active_bits & AUDIO_STREAM_URL) == AUDIO_STREAM_URL) {
    //             audio_player_stop(audio_manager_handle->url_player_handle);
    //             focus_state = AUDIO_FOCUS_LOSS;
    //         }
    //         else if ( player_active_bits & (AUDIO_STREAM_TTS + AUDIO_STREAM_PROMPT) ) {

    //         }
    //         break;
    //     case AUDIO_STREAM_TTS:
    //         break;
    //     case AUDIO_STREAM_PROMPT:
    //         break;
    // }

    return focus_state;
}



