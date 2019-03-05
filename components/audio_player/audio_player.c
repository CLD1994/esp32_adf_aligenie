// Copyright 2018 Espressif Systems (Shanghai) PTE LTD
// All rights reserved.

#include "audio_player.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "audio_def.h"
#include "audio_element.h"
#include "audio_event_iface.h"
#include "audio_hal.h"
#include "audio_mem.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "periph_spiffs.h"

#include "http_stream.h"
#include "esp_http_client.h"
#include "spiffs_stream.h"
#include "fatfs_stream.h"
#include "raw_stream.h"

#include "wav_decoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"

#include "i2s_stream.h"

#include "esp_spi_flash.h"

#define AUDIO_VOLUME "volume"

#define TAG "AudioPlayer"

#define AUDIO_HEADER_LEN 16

const static int AUDIO_PLAYER_FINISHED_BIT = BIT0;

static const char *s_source_element_tag_map[] = {
    "unknown",
    "http_reader",
    "ws_reader",
    "fat_reader",
    "spif_reader",
};

static const char *s_codec_element_tag_map[] = {
    "unknown",
    "raw_decoder",
    "wav_decoder",
    "mp3_decoder",
    "aac_decoder",
};

struct audio_player {  
    int                         rb_size;

    EventGroupHandle_t          event_group_handle;

    audio_pipeline_handle_t     pipeline_handle;

    audio_event_iface_handle_t  listener;

    audio_element_handle_t      source;

    audio_element_handle_t      codec;

    audio_element_handle_t      sink;

    audio_player_callback       callback;

    audio_src_type_t            src_type;

    audio_codec_t               codec_type;

    bool                        is_task_run;

    audio_player_state_t        player_state;
};


static void audio_player_set_state(audio_player_handle_t player_handle, audio_player_state_t state) {
    player_handle->player_state = state;
}

static void audio_player_get_state(audio_player_handle_t player_handle) {
    return player_handle->player_state;
}

static audio_codec_t audio_player_parse_codec_type_from_data(char *data) {

    if(memcmp(&data[4], "ftyp", 4) == 0) {
        ESP_LOGI(TAG, "Found M4A media");
        return AUDIO_CODEC_AAC;
    } 
    else if(memcmp(&data[0], "ID3", 3) == 0) {
        ESP_LOGI(TAG, "Found MP3 media");
        return AUDIO_CODEC_MP3;
    }
    else if(memcmp(&data[0], "RIFF", 4) == 0) {
        ESP_LOGI(TAG, "Found wav media");
        return AUDIO_CODEC_WAV;
    }
    else {
        ESP_LOGE(TAG, "Unknown media");
        return AUDIO_CODEC_NONE;
    }
}

static audio_codec_t audio_player_sniff_http_codec(char *url) {
    esp_http_client_config_t http_cfg = {
        .url = url,
        .event_handler = NULL,
        .timeout_ms = 30 * 1000,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&http_cfg);
    AUDIO_MEM_CHECK(TAG, http_client, return AUDIO_CODEC_NONE);
_stream_redirect:
    if (esp_http_client_open(http_client, 0) != ESP_OK) {
        return AUDIO_CODEC_NONE;
    }

    esp_http_client_fetch_headers(http_client);
    
    int status_code = esp_http_client_get_status_code(http_client);
    if (status_code == 301 || status_code == 302) {
        esp_http_client_set_redirection(http_client);
        goto _stream_redirect;
    }
    
    if (status_code != 200 && (esp_http_client_get_status_code(http_client) != 206)) {
        ESP_LOGE(TAG, "Invalid HTTP stream with status code[%d]", status_code);
        return AUDIO_CODEC_NONE;
    }

    char data[AUDIO_HEADER_LEN] = {0};
    int read_size = esp_http_client_read(http_client, data, sizeof(data));
    esp_http_client_cleanup(http_client);

    if (read_size != AUDIO_HEADER_LEN) {
        return AUDIO_CODEC_NONE;
    }

    return audio_player_parse_codec_type_from_data(data);
}

static esp_err_t audio_player_sniff_local_codec(const char *url, audio_codec_t *codec_type) {
    char data[AUDIO_HEADER_LEN] = {0};

    FILE *fd = fopen(url, "r");
    if(fd == NULL) {
        ESP_LOGE(TAG, "Failed to open sdcard file[%s]", url);
        return ESP_ERR_NOT_FOUND;
    }  
    fread(data, 1, sizeof(data), fd);
    fclose(fd);

    *codec_type = audio_player_parse_codec_type_from_data(data);

    return ESP_OK;
}

static void audio_player_listen_task(void *arg) {
    audio_player_handle_t player_handle = (audio_player_handle_t)arg;

    player_handle->is_task_run = true;

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    audio_event_iface_handle_t listener = player_handle->listener;

    ESP_LOGI(TAG, "[ * ] audio_player_listen_task create");

    while (player_handle->is_task_run) {

        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(listener, &msg, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "[ * ] Event interface error : %d", ret);
            continue;
        }

        if (msg.cmd == 55) {

            bool *is_success = (bool *)msg.data;

            ESP_LOGI(TAG, "[ * ] msg.cmd == 55");

            const char *source_element_tag = s_source_element_tag_map[player_handle->src_type];
            const char *codec_element_tag = s_codec_element_tag_map[player_handle->codec_type];

            const char *link_tag[] = {source_element_tag, codec_element_tag, "i2s_writer"};

            bool success = (
                audio_pipeline_link(pipeline_handle, link_tag, 3) == ESP_OK &&
                audio_pipeline_set_listener(pipeline_handle, listener) == ESP_OK
            );

            if (success) {
                *is_success = success;
            }
        }

        if (msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT) {

            if (msg.source == (void *) player_handle->source) {

                audio_element_handle_t source = player_handle->source;

                if (msg.cmd == AEL_MSG_CMD_REPORT_CODEC_FMT) {

                    ESP_LOGI(TAG, "source AEL_MSG_CMD_REPORT_CODEC_FMT");

                    audio_element_info_t source_element_info = {0};
                    audio_element_getinfo(source, &source_element_info);

                    audio_codec_t codec_fmt = source_element_info.codec_fmt;

                    if (codec_fmt == AUDIO_CODEC_NONE) {
                        codec_fmt = audio_player_sniff_http_codec(source_element_info.uri);
                    }

                    ESP_LOGI(TAG, "codec_fmt is %s", s_codec_element_tag_map[codec_fmt]);

                    if(codec_fmt != AUDIO_CODEC_NONE) {

                        if (player_handle->codec_type != codec_fmt) {

                            ESP_LOGI(TAG, "codec_fmt is change! relink! %s --> %s", 
                                s_codec_element_tag_map[player_handle->codec_type], 
                                s_codec_element_tag_map[codec_fmt]);

                            audio_pipeline_pause(pipeline_handle);
                            
                            char *source_element_tag = audio_element_get_tag(source);

                            const char *codec_element_tag = s_codec_element_tag_map[codec_fmt];

                            player_handle->codec_type = codec_fmt;

                            audio_pipeline_breakup_elements(pipeline_handle, player_handle->codec);

                            audio_pipeline_relink(pipeline_handle, (const char *[]) {source_element_tag, codec_element_tag, "i2s_writer"}, 3);
                
                            player_handle->codec = audio_pipeline_get_el_by_tag(pipeline_handle, codec_element_tag);

                            audio_pipeline_set_listener(pipeline_handle, listener);

                            audio_pipeline_run(player_handle);
                        }   
                    }
                }
                else if (msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                
                    audio_element_status_t status = (audio_element_status_t)msg.data; 

                    if (status == AEL_STATUS_STATE_RUNNING) {
                        audio_player_set_state(player_handle, PLAYER_STATE_RUNNING);
                    }
                    else if (status == AEL_STATUS_STATE_STOPPED) {

                        audio_element_state_t el_state = audio_element_get_state(source);

                        if (el_state == AEL_STATE_FINISHED) {
                            ESP_LOGI(TAG, "[ * ] source finished");
                            audio_element_stop(source);
                        }
                        else if(el_state == AEL_STATE_STOPPED) {
                            ESP_LOGI(TAG, "[ * ] source stoped");
                        }
                        else if (el_state == AEL_STATE_ERROR) {
                            ESP_LOGI(TAG, "[ * ] source error");
                            audio_element_stop(source);
                        }
                    }
                }
            } 
            else if (msg.source == (void *) player_handle->codec) {

                audio_element_handle_t codec = player_handle->codec;

                if(msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {

                    audio_element_info_t music_info = {0};
                    audio_element_getinfo(codec, &music_info);

                    ESP_LOGI(TAG, "[ * ] Receive music info from decoder, sample_rates=%d, bits=%d, ch=%d",
                                        music_info.sample_rates, music_info.bits, music_info.channels);

                    audio_element_setinfo(player_handle->sink, &music_info);
                    
                    i2s_stream_set_clk(player_handle->sink, music_info.sample_rates, music_info.bits, music_info.channels);
                }
                else if (msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {

                    audio_element_status_t status = (audio_element_status_t)msg.data; 

                    if (status == AEL_STATUS_STATE_RUNNING) {

                        //TODO resuem not AEL_MSG_CMD_REPORT_MUSIC_INFO

                        audio_player_set_state(player_handle, PLAYER_STATE_RUNNING);

                        audio_element_info_t music_info = {0};
                        audio_element_getinfo(codec, &music_info);

                        ESP_LOGI(TAG, "[ * ] Receive music info from decoder, sample_rates=%d, bits=%d, ch=%d",
                                        music_info.sample_rates, music_info.bits, music_info.channels);

                        audio_element_setinfo(player_handle->sink, &music_info);
                    
                        i2s_stream_set_clk(player_handle->sink, music_info.sample_rates, music_info.bits, music_info.channels);

                        //TODO END
                    }
                    else if(status == AEL_STATUS_STATE_STOPPED) {

                        audio_element_state_t el_state = audio_element_get_state(codec);

                        if (el_state == AEL_STATE_FINISHED) {
                            ESP_LOGI(TAG, "[ * ] codec finished");
                            audio_element_stop(codec);
                        }
                        else if(el_state == AEL_STATE_STOPPED) {
                            ESP_LOGI(TAG, "[ * ] source stoped");
                        }
                        else if (el_state == AEL_STATE_ERROR) {
                            ESP_LOGI(TAG, "[ * ] source error");
                            audio_player_set_state(player_handle, PLAYER_STATE_ERROR);
                            audio_element_stop(codec);
                        }
                    }

                }
            }
            else if (msg.source == (void *)player_handle->sink) {

                if(msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {

                    audio_element_handle_t sink = player_handle->sink;

                    audio_element_status_t status = (audio_element_status_t)msg.data;

                    audio_element_state_t el_state = audio_element_get_state(sink);

                    if (status == AEL_STATUS_STATE_PAUSED) {
                        audio_player_set_state(player_handle, PLAYER_STATE_PAUSED);
                    }
                    else if(status == AEL_STATUS_STATE_STOPPED) {

                        xEventGroupSetBits(player_handle->event_group_handle, AUDIO_PLAYER_FINISHED_BIT);

                        if (el_state == AEL_STATE_FINISHED) {
                            ESP_LOGI(TAG, "[ * ] sink finished");
                            audio_player_set_state(player_handle, PLAYER_STATE_FINISHED);
                            audio_element_stop(sink);
                        }
                        else if(el_state == AEL_STATE_STOPPED) {
                            audio_player_set_state(player_handle, PLAYER_STATE_STOPPED);
                            ESP_LOGI(TAG, "[ * ] sink stoped");
                        }
                        else if (el_state == AEL_STATE_ERROR) {
                            ESP_LOGI(TAG, "[ * ] sink error");
                            audio_player_set_state(player_handle, PLAYER_STATE_ERROR);
                            audio_element_stop(sink);
                        }
                    }

                    if (player_handle->callback){
                        player_handle->callback(player_handle, el_state);
                    }
                }
            } 
        }
    }

    vTaskDelete(NULL);
}

static esp_err_t audio_player_listen_pipeline(audio_player_handle_t player_handle) {

    ESP_LOGI(TAG, "[ 3 ] listen pipeline");

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    audio_event_iface_cfg_t listener_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();

    audio_event_iface_handle_t listener = audio_event_iface_init(&listener_cfg);
    AUDIO_MEM_CHECK(TAG, listener, return ESP_ERR_NO_MEM);

    audio_event_iface_set_listener(listener, listener);

    player_handle->listener = listener;

    if (audio_pipeline_set_listener(pipeline_handle, listener) != ESP_OK){
        goto failed;
    }

    char *task_name = "audio_player_listen_task";

    if(xTaskCreate(audio_player_listen_task, task_name, 4*1024, (void * const)player_handle, 10, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Filed to create audio player task");
        goto failed;
    }

    return ESP_OK;
failed:
    audio_pipeline_remove_listener(pipeline_handle);
    audio_event_iface_destroy(listener);
    player_handle->listener = NULL;
    return ESP_FAIL;
}

static int http_stream_event_handle(http_stream_event_msg_t *msg) {

    http_stream_event_id_t event_id = msg->event_id;
    esp_http_client_handle_t http_client = msg->http_client;
    char *buffer = msg->buffer;
    int buffer_len = msg->buffer_len;
    void *user_data = msg->user_data;
    audio_element_handle_t http_stream = msg->el;

    int ret = ESP_OK;

    switch(event_id) {
        case HTTP_STREAM_PRE_REQUEST:
            break;
        case HTTP_STREAM_ON_REQUEST:
            break;
        case HTTP_STREAM_ON_RESPONSE:
            break;
        case HTTP_STREAM_POST_REQUEST:
            break;
        case HTTP_STREAM_FINISH_REQUEST:
            break;
        case HTTP_STREAM_RESOLVE_ALL_TRACKS:
            break;
        case HTTP_STREAM_FINISH_TRACK:
            break;
        case HTTP_STREAM_FINISH_PLAYLIST:
            break;
    }

    return ret;
}

static audio_element_handle_t create_http_stream() {
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = http_stream_event_handle;
    return http_stream_init(&http_cfg);
}

static audio_element_handle_t create_ws_stream() {
    raw_stream_cfg_t ws_cfg = RAW_STREAM_CFG_DEFAULT();
    ws_cfg.type = AUDIO_STREAM_WRITER;
    return raw_stream_init(&ws_cfg);
}

static audio_element_handle_t create_fatfs_stream() {
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    return fatfs_stream_init(&fatfs_cfg);
}

static audio_element_handle_t create_spiffs_stream() {
    spiffs_stream_cfg_t spiffs_cfg = SPIFFS_STREAM_CFG_DEFAULT();
    spiffs_cfg.type = AUDIO_STREAM_READER;
    return spiffs_stream_init(&spiffs_cfg);
}

static audio_element_handle_t create_wav_decoder() {
    wav_decoder_cfg_t wav_dec_cfg = DEFAULT_WAV_DECODER_CONFIG();
    return wav_decoder_init(&wav_dec_cfg);
}

static audio_element_handle_t create_mp3_decoder() {
    mp3_decoder_cfg_t mp3_cfg = DEFAULT_MP3_DECODER_CONFIG();
    return mp3_decoder_init(&mp3_cfg);
}

static audio_element_handle_t create_aac_decoder() {
    aac_decoder_cfg_t aac_cfg = DEFAULT_AAC_DECODER_CONFIG();
    return aac_decoder_init(&aac_cfg);
}

static audio_element_handle_t create_i2s_stream() {
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;

    return i2s_stream_init(&i2s_cfg);
}


static esp_err_t audio_player_clean_pipeline_element(audio_pipeline_handle_t pipeline_handle) {

    int source_element_count = sizeof(s_source_element_tag_map) / sizeof(s_source_element_tag_map[0]);
    for(int i = 1; i < source_element_count; ++i) {
        audio_element_handle_t elements = audio_pipeline_get_el_by_tag(pipeline_handle, s_source_element_tag_map[i]);
        audio_pipeline_unregister(pipeline_handle, elements);
        audio_element_deinit(elements);
    }

    int codec_element_count = sizeof(s_codec_element_tag_map) / sizeof(s_codec_element_tag_map[0]);
    for (int i = 1; i < codec_element_count; ++i) {
        audio_element_handle_t elements = audio_pipeline_get_el_by_tag(pipeline_handle, s_codec_element_tag_map[i]);
        audio_pipeline_unregister(pipeline_handle, elements);
        audio_element_deinit(elements);
    }

    audio_element_handle_t i2s_writer = audio_pipeline_get_el_by_tag(pipeline_handle, "i2s_writer");
    audio_pipeline_unregister(pipeline_handle, i2s_writer);
    audio_element_deinit(i2s_writer);

    return ESP_OK;
}

static esp_err_t audio_player_element_register(audio_player_handle_t player_handle) {
    ESP_LOGI(TAG, "[ 2.0 ] Create all audio elements for pipeline");

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    audio_element_handle_t http_reader = NULL;
    audio_element_handle_t ws_reader = NULL;
    audio_element_handle_t fat_reader = NULL;
    audio_element_handle_t spif_reader = NULL;

    audio_element_handle_t wav_decoder = NULL;
    audio_element_handle_t mp3_decoder = NULL;
    audio_element_handle_t aac_decoder = NULL;

    audio_element_handle_t i2s_writer = NULL;

    bool is_create_success = 
        (
            (http_reader = create_http_stream())   &&
            (ws_reader   = create_ws_stream())     &&
            (fat_reader  = create_fatfs_stream())  &&
            (spif_reader = create_spiffs_stream()) &&

            (wav_decoder = create_wav_decoder())   &&
            (mp3_decoder = create_mp3_decoder())   &&
            (aac_decoder = create_aac_decoder())   &&

            (i2s_writer = create_i2s_stream())
        );

    AUDIO_MEM_CHECK(TAG, is_create_success, goto failed);

    ESP_LOGI(TAG, "[ 2.1 ] Register all audio elements to pipeline");

    bool is_register_success = 
    (
        (audio_pipeline_register(pipeline_handle, http_reader, s_source_element_tag_map[AUDIO_SRC_HTTP]) == ESP_OK) &&
        (audio_pipeline_register(pipeline_handle, ws_reader, s_source_element_tag_map[ADUIO_SRC_WEBSOCKET]) == ESP_OK) &&
        (audio_pipeline_register(pipeline_handle, fat_reader,  s_source_element_tag_map[AUDIO_SRC_SDCARD])  == ESP_OK) &&
        (audio_pipeline_register(pipeline_handle, spif_reader, s_source_element_tag_map[ADUIO_SRC_FLASH]) == ESP_OK) &&

        (audio_pipeline_register(pipeline_handle, wav_decoder, s_codec_element_tag_map[AUDIO_CODEC_WAV]) == ESP_OK) &&
        (audio_pipeline_register(pipeline_handle, mp3_decoder, s_codec_element_tag_map[AUDIO_CODEC_MP3]) == ESP_OK) &&
        (audio_pipeline_register(pipeline_handle, aac_decoder, s_codec_element_tag_map[AUDIO_CODEC_AAC]) == ESP_OK) &&

        (audio_pipeline_register(pipeline_handle, i2s_writer,  "i2s_writer")  == ESP_OK)
    );

    AUDIO_MEM_CHECK(TAG, is_register_success, goto failed);

    return ESP_OK;

failed:
    audio_player_clean_pipeline_element(pipeline_handle);
    return ESP_FAIL;
}

audio_player_handle_t audio_player_create(audio_player_cfg_t *config) {

    audio_player_handle_t player_handle = (audio_player_handle_t)audio_calloc(1, sizeof(struct audio_player));
    AUDIO_MEM_CHECK(TAG, player_handle, {
        return NULL;
    });

    player_handle->src_type = AUDIO_SRC_SDCARD;
    player_handle->codec_type = AUDIO_CODEC_MP3;
    player_handle->callback = config->callback;

    const char *source_element_tag = s_source_element_tag_map[player_handle->src_type];
    const char *codec_element_tag = s_codec_element_tag_map[player_handle->codec_type];

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();

    bool success = 
        (
            (player_handle->event_group_handle = xEventGroupCreate()) &&
            (player_handle->pipeline_handle = audio_pipeline_init(&pipeline_cfg)) &&
            (audio_player_element_register(player_handle) == ESP_ERR_AUDIO_NO_ERROR) &&
            (audio_pipeline_link(player_handle->pipeline_handle, (const char *[]) {source_element_tag, codec_element_tag, "i2s_writer"}, 3) == ESP_OK) &&
            (audio_player_listen_pipeline(player_handle) == ESP_ERR_AUDIO_NO_ERROR)
        );

    AUDIO_MEM_CHECK(TAG, success, goto create_failed);

    audio_element_handle_t source = audio_pipeline_get_el_by_tag(player_handle->pipeline_handle, source_element_tag);
    audio_element_handle_t codec = audio_pipeline_get_el_by_tag(player_handle->pipeline_handle, codec_element_tag);
    audio_element_handle_t sink = audio_pipeline_get_el_by_tag(player_handle->pipeline_handle, "i2s_writer");

    player_handle->source = source;
    player_handle->codec = codec;
    player_handle->sink = sink;

    return player_handle;
    
create_failed:
    audio_pipeline_unlink(player_handle->pipeline_handle);

    if (player_handle->pipeline_handle) {
        audio_pipeline_deinit(player_handle->pipeline_handle);
    }

    if (player_handle->event_group_handle) {
        vEventGroupDelete(player_handle->event_group_handle);
    }

    audio_free(player_handle);

    return NULL;
}

esp_err_t audio_player_destroy(audio_player_handle_t player_handle) {

    if (player_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    player_handle->is_task_run = false;

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    audio_pipeline_terminate(pipeline_handle);

    audio_pipeline_remove_listener(pipeline_handle);

    audio_event_iface_destroy(player_handle->listener);

    audio_pipeline_unlink(pipeline_handle);

    audio_player_clean_pipeline_element(pipeline_handle);

    audio_pipeline_deinit(pipeline_handle);

    vEventGroupDelete(player_handle->event_group_handle);

    audio_free(player_handle);
    
    return ESP_OK;
}

esp_err_t audio_player_start(audio_player_handle_t player_handle, const char *uri) {

    if (player_handle == NULL || uri == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!player_handle->is_task_run) {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "audio_player_start --> url = %s", uri);

    audio_player_stop(player_handle);

    xEventGroupClearBits(player_handle->event_group_handle, AUDIO_PLAYER_FINISHED_BIT);

    audio_src_type_t src_type = AUDIO_SRC_SDCARD;
    audio_codec_t codec_type = AUDIO_CODEC_MP3;

    if(strncmp(uri, "/websocket/", 11) == 0) {
        src_type = ADUIO_SRC_WEBSOCKET;
    }
    else if(strncmp(uri, "http://", 7) == 0 || strncmp(uri, "https://", 8) == 0) {
        src_type = AUDIO_SRC_HTTP;
    }
    else if(strncmp(uri, "/sdcard/", 8) == 0) {
        src_type = AUDIO_SRC_SDCARD;
        if (audio_player_sniff_local_codec(uri, &codec_type) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    else if (strncmp(uri, "/res/", 5) == 0) {
        src_type = ADUIO_SRC_FLASH;
        if (audio_player_sniff_local_codec(uri, &codec_type) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    const char *source_element_tag = s_source_element_tag_map[src_type];
    const char *codec_element_tag = s_codec_element_tag_map[codec_type];

    bool should_pipeline_relink = false;

    if (src_type != player_handle->src_type) {
        should_pipeline_relink = true;
    }else{
        if (codec_type != player_handle->codec_type) {
            should_pipeline_relink = true;
        }
    }

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    if (should_pipeline_relink) {

        ESP_LOGI(TAG, "audio_player_start, should_pipeline_relink");

        player_handle->src_type = src_type;
        player_handle->codec_type = codec_type;

        player_handle->source = audio_pipeline_get_el_by_tag(pipeline_handle, source_element_tag);
        player_handle->codec = audio_pipeline_get_el_by_tag(pipeline_handle, codec_element_tag);

        //TODO better way!
        bool is_success = false;
        audio_event_iface_msg_t msg = { 0 };
        msg.cmd = 55;
        msg.data = &is_success;
        audio_event_iface_sendout(player_handle->listener, &msg);

        while(!is_success) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        //TODO END
    }

    if (player_handle->src_type == ADUIO_SRC_WEBSOCKET) {
        if(audio_element_reset_output_ringbuf(player_handle->source) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    audio_element_info_t info = AUDIO_ELEMENT_INFO_DEFAULT();
    if(audio_element_setinfo(player_handle->source, &info) != ESP_OK) {
        return ESP_FAIL;
    }


    bool success = (
            // ( audio_element_setinfo(player_handle->source, &info) ) &&
            ( audio_element_set_uri(player_handle->source, uri) == ESP_OK ) &&
            ( audio_pipeline_run(pipeline_handle) == ESP_OK )
        );

    AUDIO_MEM_CHECK(TAG, success, return ESP_FAIL);

    return ESP_OK;
}

esp_err_t audio_player_stop(audio_player_handle_t player_handle) { 
    if (player_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    audio_pipeline_stop(pipeline_handle);

    return audio_pipeline_wait_for_stop(pipeline_handle);
}

esp_err_t audio_player_resume(audio_player_handle_t player_handle) {
    if (player_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return audio_pipeline_resume(player_handle->pipeline_handle);
}

esp_err_t audio_player_pause(audio_player_handle_t player_handle) {
    if (player_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return audio_pipeline_pause(player_handle->pipeline_handle);
}

esp_err_t audio_player_wait_for_finish(audio_player_handle_t player_handle, TickType_t ticks_to_wait) {
    esp_err_t ret = ESP_FAIL;

    EventBits_t uxBits = xEventGroupWaitBits(
        player_handle->event_group_handle, 
        AUDIO_PLAYER_FINISHED_BIT, 
        false, true, 
        ticks_to_wait);

    if (uxBits & AUDIO_PLAYER_FINISHED_BIT) {
        ret = ESP_OK;
    }

    return ret;
}

esp_err_t audio_player_ws_put_data(audio_player_handle_t player_handle, char *buffer, int buf_size) {

    if(player_handle->src_type != ADUIO_SRC_WEBSOCKET) {
        return ESP_FAIL;
    }

    return raw_stream_write(player_handle->source, buffer, buf_size);
}

esp_err_t audio_palyer_ws_put_done(audio_player_handle_t player_handle) {
    if(player_handle->src_type != ADUIO_SRC_WEBSOCKET) {
        return ESP_FAIL;
    }

    return audio_element_set_ringbuf_done(player_handle->source);
}
