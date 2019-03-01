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
#include "spiffs_stream.h"
#include "fatfs_stream.h"
#include "raw_stream.h"

#include "wav_decoder.h"
#include "mp3_decoder.h"
#include "aac_decoder.h"

#include "i2s_stream.h"

#include "esp_spi_flash.h"

#define AUDIO_VOLUME    "volume"

static const char *TAG = "AudioPlayer";

#define AUDIO_HEADER_LEN 16

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

    audio_pipeline_handle_t     pipeline_handle;

    audio_event_iface_handle_t  listener;

    audio_element_handle_t      source;

    audio_element_handle_t      codec;

    audio_element_handle_t      sink;

    audio_player_callback       callback;

    audio_src_type_t            src_type;

    audio_codec_t               codec_type;

    bool                        is_task_run;
};

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

                if (msg.cmd == AEL_MSG_CMD_REPORT_CODEC_FMT) {

                    audio_element_info_t source_element_info = {0};
                    audio_element_getinfo(player_handle->source, &source_element_info);

                    char *source_element_tag = audio_element_get_tag(player_handle->source);
                    const char *codec_element_tag = s_codec_element_tag_map[source_element_info.codec_fmt];

                    audio_pipeline_pause(pipeline_handle);

                    audio_pipeline_breakup_elements(pipeline_handle, player_handle->codec);

                    audio_pipeline_relink(pipeline_handle, (const char *[]) {source_element_tag, codec_element_tag, "i2s_writer"}, 3);
        
                    player_handle->codec = audio_pipeline_get_el_by_tag(pipeline_handle, codec_element_tag);

                    audio_pipeline_set_listener(pipeline_handle, listener);

                    audio_pipeline_run(pipeline_handle);
                }
            }
            else if (msg.source == (void *) player_handle->codec) {

                if(msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {

                    audio_element_info_t music_info = {0};
                    audio_element_getinfo(msg.source, &music_info);

                    ESP_LOGI(TAG, "[ * ] Receive music info from decoder, sample_rates=%d, bits=%d, ch=%d",
                                        music_info.sample_rates, music_info.bits, music_info.channels);

                    audio_element_setinfo(player_handle->sink, &music_info);
                    
                    i2s_stream_set_clk(player_handle->sink, music_info.sample_rates, music_info.bits, music_info.channels);
                }
                else if (msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {
                    if (player_handle->callback){
                        audio_element_status_t status = (audio_element_status_t)msg.data;
                        player_handle->callback(player_handle, status);
                    }
                }
            }
            else if (msg.source == (void *)player_handle->sink) {

                if(msg.cmd == AEL_MSG_CMD_REPORT_STATUS) {

                    audio_element_status_t status = (audio_element_status_t)msg.data;

                    if(status == AEL_STATUS_STATE_STOPPED) {

                        audio_element_state_t el_state = audio_element_get_state(msg.source);

                        if (el_state == AEL_STATE_FINISHED) {
                            ESP_LOGI(TAG, "[ * ] sink finished");
                        }
                        else if(el_state == AEL_STATE_STOPPED) {
                            ESP_LOGI(TAG, "[ * ] sink stoped");
                        }
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
    return ESP_OK;
}

static audio_element_handle_t create_http_stream() {
    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.event_handle = http_stream_event_handle;
    return http_stream_init(&http_cfg);
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
    audio_element_handle_t fat_reader = NULL;
    audio_element_handle_t spif_reader = NULL;

    audio_element_handle_t wav_decoder = NULL;
    audio_element_handle_t mp3_decoder = NULL;
    audio_element_handle_t aac_decoder = NULL;

    audio_element_handle_t i2s_writer = NULL;

    bool is_create_success = 
        (
            (http_reader = create_http_stream()) &&
            (fat_reader = create_fatfs_stream()) &&
            (spif_reader = create_spiffs_stream()) &&

            (wav_decoder = create_wav_decoder()) &&
            (mp3_decoder = create_mp3_decoder()) &&
            (aac_decoder = create_aac_decoder()) &&

            (i2s_writer = create_i2s_stream())
        );

    AUDIO_MEM_CHECK(TAG, is_create_success, goto failed);

    ESP_LOGI(TAG, "[ 2.1 ] Register all audio elements to pipeline");

    bool is_register_success = 
    (
        (audio_pipeline_register(pipeline_handle, http_reader, s_source_element_tag_map[AUDIO_SRC_HTTP]) == ESP_OK) &&
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

static esp_err_t audio_player_source_sniff(const char *url, audio_codec_t *codec_type) {
    char data[AUDIO_HEADER_LEN] = {0};

    FILE *fd = fopen(url, "r");
    if(fd == NULL) {
        ESP_LOGE(TAG, "Failed to open sdcard file[%s]", url);
        return ESP_ERR_NOT_FOUND;
    }  
    fread(data, 1, sizeof(data), fd);

    if(memcmp(&data[4], "ftyp", 4) == 0) 
    {
        ESP_LOGI(TAG, "Found M4A media");
        *codec_type = AUDIO_CODEC_AAC;
    } 
    else if(memcmp(&data[0], "ID3", 3) == 0) 
    {
        ESP_LOGI(TAG, "Found MP3 media");
        *codec_type = AUDIO_CODEC_MP3;
    }
    else if(memcmp(&data[0], "RIFF", 4) == 0) 
    {
        ESP_LOGI(TAG, "Found wav media");
        *codec_type = AUDIO_CODEC_WAV;
    }else {
        ESP_LOGI(TAG, "Default MP3 media");
        *codec_type = AUDIO_CODEC_MP3;
    }

    fclose(fd);

    return ESP_OK;
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

    audio_free(player_handle);
    
    return ESP_OK;
}

esp_err_t audio_player_start(audio_player_handle_t player_handle, const char *url) {

    if (player_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!player_handle->is_task_run) {
        return ESP_FAIL;
    }

    audio_pipeline_handle_t pipeline_handle = player_handle->pipeline_handle;

    audio_src_type_t src_type = AUDIO_SRC_SDCARD;
    audio_codec_t codec_type = AUDIO_CODEC_MP3;

    if(strncmp(url, "/websocket/", 11) == 0) {
        src_type = ADUIO_SRC_WEBSOCKET;
    }
    else if(strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0) {
        src_type = AUDIO_SRC_HTTP;
    }
    else if(strncmp(url, "/sdcard/", 8) == 0) {
        src_type = AUDIO_SRC_SDCARD;
        if (audio_player_source_sniff(url, &codec_type) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    else if (strncmp(url, "/res/", 5) == 0) {
        src_type = ADUIO_SRC_FLASH;
        if (audio_player_source_sniff(url, &codec_type) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    const char *source_element_tag = s_source_element_tag_map[src_type];
    const char *codec_element_tag = s_codec_element_tag_map[codec_type];

    audio_pipeline_stop(pipeline_handle);
    audio_pipeline_wait_for_stop(pipeline_handle);

    player_handle->src_type = src_type;
    player_handle->codec_type = codec_type;

    player_handle->source = audio_pipeline_get_el_by_tag(pipeline_handle, source_element_tag);
    player_handle->codec = audio_pipeline_get_el_by_tag(pipeline_handle, codec_element_tag);

    bool is_success = false;

    audio_event_iface_msg_t msg = { 0 };
    msg.cmd = 55;
    msg.data = &is_success;
    audio_event_iface_sendout(player_handle->listener, &msg);

    while(!is_success) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }


    bool success = (
            audio_element_set_uri(player_handle->source, url) == ESP_OK &&
            audio_pipeline_run(pipeline_handle) == ESP_OK
        );

    AUDIO_MEM_CHECK(TAG, success, return ESP_FAIL);


    return ESP_OK;
}

esp_err_t audio_player_stop(audio_player_handle_t player_handle) {
    if (player_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return audio_pipeline_stop(player_handle->pipeline_handle);
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
