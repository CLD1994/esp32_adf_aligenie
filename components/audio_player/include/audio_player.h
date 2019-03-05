#ifndef _URANUS_AUDIO_PLAYER_H
#define _URANUS_AUDIO_PLAYER_H

#include "audio_element.h"
#include "audio_pipeline.h"

typedef enum {
    PLAYER_STATE_INIT = 0,
    PLAYER_STATE_RUNNING,
    PLAYER_STATE_PAUSED,
    PLAYER_STATE_STOPPED,
    PLAYER_STATE_FINISHED,
    PLAYER_STATE_ERROR
} audio_player_state_t;

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

esp_err_t audio_player_start(audio_player_handle_t player_handle, const char *uri);
esp_err_t audio_player_stop(audio_player_handle_t player_handle);
esp_err_t audio_player_resume(audio_player_handle_t player_handle);
esp_err_t audio_player_pause(audio_player_handle_t player_handle);
esp_err_t audio_player_wait_for_finish(audio_player_handle_t player_handle, TickType_t ticks_to_wait);

esp_err_t audio_player_ws_put_data(audio_player_handle_t player_handle, char *buffer, int buf_size);
esp_err_t audio_palyer_ws_put_done(audio_player_handle_t player_handle);

#endif

