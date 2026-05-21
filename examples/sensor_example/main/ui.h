#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*on_beep)(void);
    void (*on_ir_send)(void);
} ui_callbacks_t;

void ui_create(const ui_callbacks_t *cbs);

void ui_imu_update(float ax, float ay, float az,
                   float gx, float gy, float gz,
                   float tilt_deg);

void ui_humiture_update(float temp_c, float humidity_pct);
void ui_humiture_set_status(const char *msg);

void ui_radar_set_present(bool present, uint32_t event_count);

void ui_audio_set_mic_rms(int rms);
void ui_audio_set_status(const char *msg);

void ui_ir_update(uint32_t rx_pulses, uint32_t last_rx_ms);
void ui_ir_set_status(const char *msg);

void ui_buttons_update(int idx, uint32_t short_count, uint32_t long_count);

void ui_uptime_update(uint64_t seconds);

#ifdef __cplusplus
}
#endif
