#include "audio_check.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_codec_dev.h"

#include "bsp/esp-box-3.h"

#include "ui.h"

static const char *TAG = "audio";

#define SAMPLE_RATE      16000
#define BITS_PER_SAMPLE  16
#define MIC_CHUNK_FRAMES 512
#define BEEP_FREQ_HZ     1000
#define BEEP_DURATION_MS 200
#define BEEP_VOLUME_PCT  60

static esp_codec_dev_handle_t s_mic;
static esp_codec_dev_handle_t s_spk;
static TaskHandle_t s_beep_task_handle;

static int compute_rms(const int16_t *samples, int frames)
{
    if (frames <= 0) {
        return 0;
    }
    int64_t sum_sq = 0;
    for (int i = 0; i < frames; i++) {
        int32_t s = samples[i];
        sum_sq += (int64_t)s * s;
    }
    return (int)sqrtf((float)(sum_sq / frames));
}

static void mic_task(void *arg)
{
    (void)arg;
    int16_t *buf = malloc(MIC_CHUNK_FRAMES * sizeof(int16_t));
    if (!buf) {
        ESP_LOGE(TAG, "mic buffer alloc failed");
        ui_audio_set_status("mic alloc fail");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        int err = esp_codec_dev_read(s_mic, buf,
                                     MIC_CHUNK_FRAMES * sizeof(int16_t));
        if (err == ESP_CODEC_DEV_OK) {
            int rms = compute_rms(buf, MIC_CHUNK_FRAMES);
            ui_audio_set_mic_rms(rms);
        } else {
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

static void beep_task(void *arg)
{
    (void)arg;
    s_beep_task_handle = xTaskGetCurrentTaskHandle();

    const int total_frames = (SAMPLE_RATE * BEEP_DURATION_MS) / 1000;
    int16_t *tone = malloc(total_frames * sizeof(int16_t));
    if (!tone) {
        ui_audio_set_status("beep alloc fail");
        s_beep_task_handle = NULL;
        vTaskDelete(NULL);
        return;
    }

    const float two_pi_f = 2.0f * (float)M_PI;
    const float k = two_pi_f * (float)BEEP_FREQ_HZ / (float)SAMPLE_RATE;
    const float amp = 12000.0f;
    for (int i = 0; i < total_frames; i++) {
        tone[i] = (int16_t)(amp * sinf(k * (float)i));
    }

    ui_audio_set_status("beeping...");
    esp_codec_dev_set_out_vol(s_spk, BEEP_VOLUME_PCT);
    int err = esp_codec_dev_write(s_spk, tone,
                                  total_frames * (int)sizeof(int16_t));
    free(tone);
    if (err != ESP_CODEC_DEV_OK) {
        ui_audio_set_status("beep write fail");
    } else {
        ui_audio_set_status("beep done");
    }

    s_beep_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_check_init(void)
{
    esp_err_t err = bsp_audio_init(NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_audio_init: %s", esp_err_to_name(err));
        ui_audio_set_status("audio init fail");
        return err;
    }

    s_spk = bsp_audio_codec_speaker_init();
    s_mic = bsp_audio_codec_microphone_init();
    if (!s_spk || !s_mic) {
        ESP_LOGE(TAG, "codec init failed (spk=%p mic=%p)", s_spk, s_mic);
        ui_audio_set_status("codec init fail");
        return ESP_FAIL;
    }

    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = BITS_PER_SAMPLE,
        .channel         = 1,
        .channel_mask    = 0,
        .sample_rate     = SAMPLE_RATE,
        .mclk_multiple   = 0,
    };
    esp_codec_dev_set_in_gain(s_mic, 42.0f);
    if (esp_codec_dev_open(s_mic, &fs) != 0) {
        ui_audio_set_status("mic open fail");
        return ESP_FAIL;
    }
    if (esp_codec_dev_open(s_spk, &fs) != 0) {
        ui_audio_set_status("spk open fail");
        return ESP_FAIL;
    }

    BaseType_t ok = xTaskCreatePinnedToCore(mic_task, "mic", 4096, NULL,
                                            4, NULL, 1);
    if (ok != pdPASS) {
        ui_audio_set_status("mic task fail");
        return ESP_FAIL;
    }

    ui_audio_set_status("mic running 16kHz");
    return ESP_OK;
}

void audio_check_beep(void)
{
    if (s_beep_task_handle != NULL) {
        return;
    }
    xTaskCreate(beep_task, "beep", 4096, NULL, 4, NULL);
}
