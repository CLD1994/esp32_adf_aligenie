#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"

#include "audio_mem.h"
#include "esp_audio.h"
#include "esp_log.h"
#include "i2s_stream.h"
#include "audio_recorder.h"


static const char *TAG = "AudioRec";

int audio_recorder_data_get(void *handle, char *data, int data_size)
{
    audio_element_handle_t el = (audio_element_handle_t)handle;

    ringbuf_handle_t rb = audio_element_get_output_ringbuf(el);
    int size_read = rb_read(rb, data, data_size, pdMS_TO_TICKS(REC_PERIOD_TIME_MS));
    
    return size_read;
}

int audio_recorder_stop(void *handle)
{
    audio_element_handle_t el = (audio_element_handle_t)handle;
    ringbuf_handle_t rb = audio_element_get_output_ringbuf(el);
    
    audio_element_deinit(el);
    if(rb != NULL){
        rb_destroy(rb);
    }
    
    return ESP_OK;
}

void* audio_recorder_start(int sample_rate)
{
    audio_element_handle_t i2s_stream_reader = NULL;
    ringbuf_handle_t rb_mic2codec = NULL;
        
    audio_hal_codec_config_t audio_hal_codec_cfg =  AUDIO_HAL_ES8388_DEFAULT();
    audio_hal_codec_cfg.i2s_iface.samples = AUDIO_HAL_16K_SAMPLES;
    audio_hal_handle_t audio_hal = audio_hal_init(&audio_hal_codec_cfg, 0);
    if(audio_hal == NULL){
        ESP_LOGE(TAG, "Failed to init audio hal codec");
        return NULL;
    }

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_READER;
    i2s_cfg.i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_cfg.i2s_config.sample_rate= sample_rate;
    i2s_stream_reader = i2s_stream_init(&i2s_cfg);

    rb_mic2codec = rb_create(REC_PERIOD_SIZE * REC_PERIOD_CNT, 1);
    audio_element_set_output_ringbuf(i2s_stream_reader, rb_mic2codec);
    //audio_element_set_output_timeout(i2s_stream_reader, pdMS_TO_TICKS(REC_PERIOD_TIME_MS * REC_PERIOD_CNT));
    audio_element_set_output_timeout(i2s_stream_reader, portMAX_DELAY);

    audio_element_run(i2s_stream_reader);
    audio_element_resume(i2s_stream_reader, 0, portMAX_DELAY);
    
    ESP_LOGI(TAG, "Recorder start success");
    
    return (void*)i2s_stream_reader;
}

void audio_recorder_test()
{
    void *recorder = audio_recorder_start(16000);

    char data[512];

    FILE *fd = fopen("/sdcard/record.pcm", "wb");
    if (fd == NULL) {
        ESP_LOGE(TAG, "Filed to open file");
        return;
    }
    
    int k = 512;
    while(k > 0) {
        int byte_read = audio_recorder_data_get(recorder, data, 512);
        k--;

        fwrite(data, 1, byte_read, fd);
    }
    audio_recorder_stop(recorder);
    fclose(fd);

    ESP_LOGI(TAG, "Recorder finished");
}

