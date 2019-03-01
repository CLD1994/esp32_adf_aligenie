#ifndef _URANUS_AUDIO_MANAGER_H
#define _URANUS_AUDIO_MANAGER_H

#include "esp_err.h"
#include "audio_player.h"

typedef enum {
    AUDIO_FOCUS_LOSS = 0,
    AUDIO_FOCUS_GAIN,
    AUDIO_FOCUS_CAN_DUCK,
} audio_focus_state_t;

typedef enum {
    AUDIO_STREAM_UNKNOWN = 0,
    AUDIO_STREAM_URL = 1,
    AUDIO_STREAM_TTS = 2,
    AUDIO_STREAM_PROMPT = 4,
} audio_player_type_t;

typedef esp_err_t (*audio_manager_state_callback)(audio_player_handle_t player_handle, audio_element_state_t status);

typedef struct {
    audio_manager_state_callback state_callback;
} audio_manager_cfg_t;

typedef struct audio_manager *audio_manager_handle_t;


esp_err_t audio_manager_init(audio_manager_cfg_t *config);
esp_err_t audio_manager_deinit();

esp_err_t audio_manager_start(audio_player_type_t type, const char *url);
esp_err_t audio_manager_stop(audio_player_type_t type);
esp_err_t audio_manager_resume(audio_player_type_t type);
esp_err_t audio_manager_pause(audio_player_type_t type);

audio_focus_state_t audio_manager_request_focus(audio_player_type_t type);

#endif
