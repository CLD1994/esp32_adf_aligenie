#include "string.h"

#include "esp_log.h"
#include "aligenie_audio.h"
#include "aligenie_os.h"
#include "audio_recorder.h"

#include "audio_manager.h"

#define TAG         "ag_audio"

static AG_AUDIO_INFO_T *s_ptr_http_audio_info = NULL;

static void on_audio_manager_state_change(audio_player_type_t type, audio_element_state_t status) {
    
    if (type == AUDIO_STREAM_URL) {
        switch(status) 
        {
            case AEL_STATE_RUNNING:
                ESP_LOGI(TAG, "url player running");
                s_ptr_http_audio_info->status = AG_PLAYER_URL_PLAYING;
                break;
                
            case AEL_STATE_PAUSED:
                ESP_LOGI(TAG, "url player paused");
                s_ptr_http_audio_info->status = AG_PLAYER_URL_PAUSED;
                break;
                
            case AEL_STATE_STOPPED:
                ESP_LOGI(TAG, "url player stopped");
                s_ptr_http_audio_info->status = AG_PLAYER_URL_STOPPED;
                break;
                
            case AEL_STATE_FINISHED:
                ESP_LOGI(TAG, "url player finished");
                s_ptr_http_audio_info->status = AG_PLAYER_URL_FINISHED;
                break;
       
            default:
                break;
        }
        ag_sdk_notify_player_status_change(s_ptr_http_audio_info);
    }
}

void ag_audio_idle() {
    ESP_LOGI(TAG, "ag_audio_idle()");
}

int ag_audio_url_play(const AG_AUDIO_INFO_T * const info) {
    ESP_LOGI(TAG, "ag_audio_url_play");

    if(s_ptr_http_audio_info == NULL) {
        s_ptr_http_audio_info = (AG_AUDIO_INFO_T *)AG_OS_CALLOC(1, sizeof(AG_AUDIO_INFO_T));
    }

    int ret = -1;

    if (s_ptr_http_audio_info){
        memcpy(s_ptr_http_audio_info, info, sizeof(AG_AUDIO_INFO_T));
        audio_manager_register_state_callback(on_audio_manager_state_change);
        ret = audio_manager_start(AUDIO_STREAM_URL, info->url);
    }

    return ret;
}

int ag_audio_url_pause() {
    ESP_LOGI(TAG, "ag_audio_url_pause");
    audio_manager_pause(AUDIO_STREAM_URL);
    return 0;
}

int ag_audio_url_resume() {
    ESP_LOGI(TAG, "ag_audio_url_resume");
    audio_manager_resume(AUDIO_STREAM_URL);
    return 0;
}

int ag_audio_url_stop() {
    ESP_LOGI(TAG, "ag_audio_url_stop");
    audio_manager_stop(AUDIO_STREAM_URL);
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
    return s_ptr_http_audio_info->status;

}

int ag_audio_play_prompt(char * url) {
    ESP_LOGI(TAG, "ag_audio_play_prompt");
    return audio_manager_start(AUDIO_STREAM_PROMPT, url);
}

int ag_audio_play_local_prompt(AG_LOCAL_PROMPT_TYPE_E type) {
    ESP_LOGI(TAG, "ag_audio_play_local_prompt");
    return 0;

}

int ag_audio_stop_prompt() {
    ESP_LOGI(TAG, "ag_audio_stop_prompt");
    return audio_manager_stop(AUDIO_STREAM_PROMPT);

}

int ag_audio_tts_play_start(AG_TTS_FORMAT_E format) {
    ESP_LOGI(TAG, "ag_audio_tts_play_start");
    return audio_manager_start(AUDIO_STREAM_TTS, "/websocket/tts.mp3");
}

int ag_audio_tts_play_put_data(const void * const inData, int dataLen) {
    int len = audio_manager_ws_put_data((char *)inData, dataLen);
    ESP_LOGI(TAG, "ag_audio_tts_play_put_data len = %d", len);
    return 0;

}

int ag_audio_tts_play_wait_finish() {
    ESP_LOGI(TAG, "ag_audio_tts_play_wait_finish");
    if(audio_manager_ws_put_done() != 0) {
        ESP_LOGE(TAG, "audio_manager_ws_put_done error");
        return -1;
    }

    return audio_manager_wait_for_finish(AUDIO_STREAM_TTS, portMAX_DELAY);
}

int ag_audio_tts_play_abort() {
    ESP_LOGI(TAG, "ag_audio_tts_play_abort");
    return audio_manager_stop(AUDIO_STREAM_TTS);

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
