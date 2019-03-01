#include "string.h"

#include "esp_log.h"
#include "aligenie_audio.h"
#include "audio_recorder.h"

#include "audio_manager.h"

#define TAG         "ag_audio"

void ag_audio_idle() {
    ESP_LOGI(TAG, "ag_audio_idle()");
    return 0;
}

int ag_audio_url_play(const AG_AUDIO_INFO_T * const info) {
    ESP_LOGI(TAG, "ag_audio_url_play");



    return 0;
}

int ag_audio_url_pause() {
    ESP_LOGI(TAG, "ag_audio_url_pause");
    return 0;
}

int ag_audio_url_resume() {
    ESP_LOGI(TAG, "ag_audio_url_resume");
    return 0;
}

int ag_audio_url_stop() {
    ESP_LOGI(TAG, "ag_audio_url_stop");
    return 0;
}

int ag_audio_get_url_play_progress(const int * outval) {
    ESP_LOGI(TAG, "ag_audio_get_url_play_progress");
    return 0;
}

int ag_audio_get_url_play_duration(const int * outval) {
    ESP_LOGI(TAG, "ag_audio_get_url_play_progress");
    return 0;
}

int ag_audio_url_seek_progress(int seconds) {
    ESP_LOGI(TAG, "ag_audio_url_seek_progress");
    return 0;

}

AG_PLAYER_STATUS_E ag_audio_get_player_status() {
    ESP_LOGI(TAG, "ag_audio_get_player_status");
    return 0;

}

int ag_audio_play_prompt(char * url) {
    ESP_LOGI(TAG, "ag_audio_play_prompt");
    return 0;

}

int ag_audio_play_local_prompt(AG_LOCAL_PROMPT_TYPE_E type) {
    ESP_LOGI(TAG, "ag_audio_play_local_prompt");
    return 0;

}

int ag_audio_stop_prompt() {
    ESP_LOGI(TAG, "ag_audio_stop_prompt");
    return 0;

}

int ag_audio_tts_play_start(AG_TTS_FORMAT_E format) {
    ESP_LOGI(TAG, "ag_audio_tts_play_start");
    return audio_manager_start(AUDIO_STREAM_TTS, "/websocket/tts.mp3");
}

int ag_audio_tts_play_put_data(const void * const inData, int dataLen) {
    ESP_LOGI(TAG, "ag_audio_tts_play_put_data");
    return 0;

}

int ag_audio_tts_play_wait_finish() {
    ESP_LOGI(TAG, "ag_audio_tts_play_wait_finish");
    return 0;

}

int ag_audio_tts_play_abort() {
    ESP_LOGI(TAG, "ag_audio_tts_play_abort");
    return 0;

}

AG_RECORDER_DATA_TYPE_E ag_audio_get_record_data_type() {
    return AG_RECORDER_DATA_TYPE_PCM;
}

static void* recorder_handle = NULL;

int ag_audio_record_start(AG_AUDIO_RECORD_TYPE_E type) {

    ESP_LOGI(TAG, "ag_audio_record_start");

    recorder_handle = audio_recorder_start(16000);
    
    if(recorder_handle == NULL) {
        return -1;
    }
    return 0;
}

int ag_audio_record_get_data(const void * outData, int maxLen) {
    ESP_LOGI(TAG, "ag_audio_record_get_data");
    return audio_recorder_data_get(recorder_handle, outData, maxLen);
}

int ag_audio_record_stop() {
    ESP_LOGI(TAG, "ag_audio_record_stop");
    int ret = audio_recorder_stop(recorder_handle);
    recorder_handle = NULL;

    if(ret != 0) {
        return -1;
    }
    return 0;
}

int ag_audio_set_volume(int volume) {
    ESP_LOGI(TAG, "ag_audio_set_volume");
    return 0;
}

int ag_audio_get_volume() {
    ESP_LOGI(TAG, "ag_audio_get_volume");
    return 90;
}

int ag_audio_vad_start() {
    ESP_LOGI(TAG, "ag_audio_vad_start");
    return 0;
}
