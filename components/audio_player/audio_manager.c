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

static esp_err_t url_audio_player_callback(audio_player_handle_t player_handle, audio_element_state_t status) {
    if(s_audio_manager_handle->state_callback) {
        s_audio_manager_handle->state_callback(AUDIO_STREAM_URL, status);
    }

    return ESP_OK;
}

static esp_err_t tts_audio_player_callback(audio_player_handle_t player_handle, audio_element_state_t status) {
    if(s_audio_manager_handle->state_callback) {
        s_audio_manager_handle->state_callback(AUDIO_STREAM_TTS, status);
    }

    return ESP_OK;
}

static esp_err_t prompt_audio_player_callback(audio_player_handle_t player_handle, audio_element_state_t status) {
    if(s_audio_manager_handle->state_callback) {
        s_audio_manager_handle->state_callback(AUDIO_STREAM_PROMPT, status);
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

    audio_player_cfg_t url_player_cfg = {
        .rb_size = 8*1024,
        .callback = url_audio_player_callback,
    };

    audio_player_cfg_t tts_player_cfg = {
        .rb_size = 8*1024,
        .callback = tts_audio_player_callback,
    };
    audio_player_cfg_t prompt_player_cfg = {
        .rb_size = 8*1024,
        .callback = prompt_audio_player_callback,
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

    audio_free(s_audio_manager_handle);
    s_audio_manager_handle = NULL;
    return ESP_OK;
}

esp_err_t audio_manager_start(audio_player_type_t type, const char *uri) {

    esp_err_t err = ESP_FAIL;

    switch(type) {
        case AUDIO_STREAM_URL:
            err = audio_player_start(s_audio_manager_handle->url_player_handle, uri);
            break;
        case AUDIO_STREAM_TTS:
            err = audio_player_start(s_audio_manager_handle->tts_player_handle, uri);
            break;
        case AUDIO_STREAM_PROMPT:
            err = audio_player_start(s_audio_manager_handle->prompt_player_handle, uri);
            break;
        default:
            err = ESP_FAIL;
            break;
    }

    return err;
}

esp_err_t audio_manager_stop(audio_player_type_t type) {

    esp_err_t err = ESP_FAIL;

    switch(type) {
        case AUDIO_STREAM_URL:
            err = audio_player_stop(s_audio_manager_handle->url_player_handle);
            break;
        case AUDIO_STREAM_TTS:
            err = audio_player_stop(s_audio_manager_handle->tts_player_handle);
            break;
        case AUDIO_STREAM_PROMPT:
            err = audio_player_stop(s_audio_manager_handle->prompt_player_handle);
            break;
        default:
            err = ESP_FAIL;
            break;
    }

    return err;
}

esp_err_t audio_manager_resume(audio_player_type_t type) {

    esp_err_t err = ESP_FAIL;

    switch(type) {
        case AUDIO_STREAM_URL:
            err = audio_player_resume(s_audio_manager_handle->url_player_handle);
            break;
        case AUDIO_STREAM_TTS:
            err = audio_player_resume(s_audio_manager_handle->tts_player_handle);
            break;
        case AUDIO_STREAM_PROMPT:
            err = audio_player_resume(s_audio_manager_handle->prompt_player_handle);
            break;
        default:
            err = ESP_FAIL;
            break;
    }
    return err;
}

esp_err_t audio_manager_pause(audio_player_type_t type) {

    esp_err_t err = ESP_FAIL;

    switch(type) {
        case AUDIO_STREAM_URL:
            err = audio_player_pause(s_audio_manager_handle->url_player_handle);
            break;
        case AUDIO_STREAM_TTS:
            err = audio_player_pause(s_audio_manager_handle->tts_player_handle);
            break;
        case AUDIO_STREAM_PROMPT:
            err = audio_player_pause(s_audio_manager_handle->prompt_player_handle);
            break;
        default:
            err = ESP_FAIL;
            break;
    }
    return err;
}

esp_err_t audio_manager_ws_put_data(char *buffer, int buf_size) {
    return audio_player_ws_put_data(s_audio_manager_handle->tts_player_handle, buffer, buf_size);
}

esp_err_t audio_manager_ws_put_done() {
    return audio_palyer_ws_put_done(s_audio_manager_handle->tts_player_handle);
}

esp_err_t audio_manager_wait_for_finish(audio_player_type_t type, TickType_t ticks_to_wait) {

    audio_player_handle_t player_handle;

    switch(type) {
        case AUDIO_STREAM_URL:
            player_handle = s_audio_manager_handle->url_player_handle;
            break;
        case AUDIO_STREAM_TTS:
            player_handle = s_audio_manager_handle->tts_player_handle;
            break;
        case AUDIO_STREAM_PROMPT:
            player_handle = s_audio_manager_handle->prompt_player_handle;
            break;
        default:
            player_handle = NULL;
            break;
    }

    return audio_player_wait_for_finish(player_handle, ticks_to_wait);
}

void audio_manager_register_state_callback(audio_manager_state_callback state_callback) {
    s_audio_manager_handle->state_callback = state_callback;
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



