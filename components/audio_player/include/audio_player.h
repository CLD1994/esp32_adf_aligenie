#ifndef _URANUS_AUDIO_SERVICE_H
#define _URANUS_AUDIO_SERVICE_H

#include "audio_element.h"
#include "audio_pipeline.h"

typedef enum {
    AUDIO_SRC_UNKNOWN = 0,
    AUDIO_SRC_HTTP,
    ADUIO_SRC_WEBSOCKET,
    AUDIO_SRC_SDCARD,
    ADUIO_SRC_FLASH,
} audio_src_type_t;

typedef struct audio_player *audio_player_handle_t;

typedef esp_err_t (*audio_player_callback)(audio_player_handle_t player_handle, audio_element_state_t status);

typedef struct {
    int rb_size;
    audio_player_callback callback;
} audio_player_cfg_t;

audio_player_handle_t audio_player_create(audio_player_cfg_t *config);
esp_err_t audio_player_destroy(audio_player_handle_t player_handle);

esp_err_t audio_player_start(audio_player_handle_t player_handle, const char *url);
esp_err_t audio_player_stop(audio_player_handle_t player_handle);
esp_err_t audio_player_resume(audio_player_handle_t player_handle);
esp_err_t audio_player_pause(audio_player_handle_t player_handle);

#endif

