#include "ui.h"

#include <stdbool.h>
#include <stdio.h>

#include "esp_log.h"
#include "bsp/esp-box-3.h"
#include "lvgl.h"

static const char *TAG = "ui";

static ui_callbacks_t s_cbs;

static lv_obj_t *s_tabview;

static lv_obj_t *s_lbl_acc_x;
static lv_obj_t *s_lbl_acc_y;
static lv_obj_t *s_lbl_acc_z;
static lv_obj_t *s_lbl_gyro_x;
static lv_obj_t *s_lbl_gyro_y;
static lv_obj_t *s_lbl_gyro_z;
static lv_obj_t *s_bar_tilt;

static lv_obj_t *s_lbl_temp;
static lv_obj_t *s_lbl_hum;
static lv_obj_t *s_lbl_humiture_status;

static lv_obj_t *s_lbl_radar_state;
static lv_obj_t *s_lbl_radar_count;

static lv_obj_t *s_bar_mic_rms;
static lv_obj_t *s_lbl_mic_rms;
static lv_obj_t *s_lbl_audio_status;

static lv_obj_t *s_lbl_ir_rx;
static lv_obj_t *s_lbl_ir_last;
static lv_obj_t *s_lbl_ir_status;

static lv_obj_t *s_lbl_btn[3];

static lv_obj_t *s_lbl_uptime;

#define COLOR_ACCENT   lv_color_hex(0x00E5FF)
#define COLOR_OK       lv_color_hex(0x06D6A0)
#define COLOR_WARN     lv_color_hex(0xFFD166)
#define COLOR_MUTED    lv_color_hex(0x9CA3AF)
#define COLOR_BG       lv_color_hex(0x0A0E27)
#define COLOR_BAR_BG   lv_color_hex(0x1F2937)
#define COLOR_PINK     lv_color_hex(0xEF476F)

static lv_obj_t *make_value_label(lv_obj_t *parent, const char *initial)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, initial);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    return lbl;
}

static lv_obj_t *make_section_title(lv_obj_t *parent, const char *text,
                                    lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

static void beep_btn_event(lv_event_t *e)
{
    (void)e;
    if (s_cbs.on_beep) {
        s_cbs.on_beep();
    }
}

static void ir_send_btn_event(lv_event_t *e)
{
    (void)e;
    if (s_cbs.on_ir_send) {
        s_cbs.on_ir_send();
    }
}

static void build_imu_tab(lv_obj_t *tab)
{
    lv_obj_t *title_acc = make_section_title(tab, "Accel (g)", COLOR_WARN);
    lv_obj_align(title_acc, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_acc_x = make_value_label(tab, "X: ---");
    s_lbl_acc_y = make_value_label(tab, "Y: ---");
    s_lbl_acc_z = make_value_label(tab, "Z: ---");
    lv_obj_align(s_lbl_acc_x, LV_ALIGN_TOP_LEFT, 0, 22);
    lv_obj_align(s_lbl_acc_y, LV_ALIGN_TOP_LEFT, 0, 42);
    lv_obj_align(s_lbl_acc_z, LV_ALIGN_TOP_LEFT, 0, 62);

    lv_obj_t *title_gyro = make_section_title(tab, "Gyro (dps)", COLOR_PINK);
    lv_obj_align(title_gyro, LV_ALIGN_TOP_LEFT, 150, 0);

    s_lbl_gyro_x = make_value_label(tab, "X: ---");
    s_lbl_gyro_y = make_value_label(tab, "Y: ---");
    s_lbl_gyro_z = make_value_label(tab, "Z: ---");
    lv_obj_align(s_lbl_gyro_x, LV_ALIGN_TOP_LEFT, 150, 22);
    lv_obj_align(s_lbl_gyro_y, LV_ALIGN_TOP_LEFT, 150, 42);
    lv_obj_align(s_lbl_gyro_z, LV_ALIGN_TOP_LEFT, 150, 62);

    lv_obj_t *title_tilt = make_section_title(tab, "Tilt (0-90 deg)", COLOR_OK);
    lv_obj_align(title_tilt, LV_ALIGN_TOP_LEFT, 0, 95);

    s_bar_tilt = lv_bar_create(tab);
    lv_obj_set_size(s_bar_tilt, 280, 20);
    lv_obj_align(s_bar_tilt, LV_ALIGN_TOP_LEFT, 0, 118);
    lv_bar_set_range(s_bar_tilt, 0, 90);
    lv_obj_set_style_bg_color(s_bar_tilt, COLOR_BAR_BG, 0);
    lv_obj_set_style_bg_color(s_bar_tilt, COLOR_OK, LV_PART_INDICATOR);
}

static void build_humiture_tab(lv_obj_t *tab)
{
    lv_obj_t *title = make_section_title(tab, "AHT30 (dock I2C)", COLOR_ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_temp = make_value_label(tab, "Temp: ---");
    s_lbl_hum  = make_value_label(tab, "Hum:  ---");
    lv_obj_set_style_text_font(s_lbl_temp, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_font(s_lbl_hum,  &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_temp, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_obj_align(s_lbl_hum,  LV_ALIGN_TOP_LEFT, 0, 60);

    s_lbl_humiture_status = make_value_label(tab, "status: waiting...");
    lv_obj_set_style_text_color(s_lbl_humiture_status, COLOR_MUTED, 0);
    lv_obj_align(s_lbl_humiture_status, LV_ALIGN_TOP_LEFT, 0, 100);
}

static void build_radar_tab(lv_obj_t *tab)
{
    lv_obj_t *title = make_section_title(tab, "AT581X radar (GPIO 21 INT)",
                                         COLOR_ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_radar_state = make_value_label(tab, "State: NO PRESENCE");
    lv_obj_align(s_lbl_radar_state, LV_ALIGN_TOP_LEFT, 0, 30);

    s_lbl_radar_count = make_value_label(tab, "Events: 0");
    lv_obj_set_style_text_color(s_lbl_radar_count, COLOR_MUTED, 0);
    lv_obj_align(s_lbl_radar_count, LV_ALIGN_TOP_LEFT, 0, 60);

    lv_obj_t *hint = lv_label_create(tab);
    lv_label_set_text(hint,
                      "Wave hand within 1-2m of the radar module.\n"
                      "INT pin pulses on detection.");
    lv_obj_set_style_text_color(hint, COLOR_MUTED, 0);
    lv_obj_set_width(hint, 280);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 90);
}

static void build_audio_tab(lv_obj_t *tab)
{
    lv_obj_t *title = make_section_title(tab, "Mic RMS / Speaker", COLOR_ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_bar_mic_rms = lv_bar_create(tab);
    lv_obj_set_size(s_bar_mic_rms, 200, 20);
    lv_obj_align(s_bar_mic_rms, LV_ALIGN_TOP_LEFT, 0, 30);
    lv_bar_set_range(s_bar_mic_rms, 0, 8000);
    lv_obj_set_style_bg_color(s_bar_mic_rms, COLOR_BAR_BG, 0);
    lv_obj_set_style_bg_color(s_bar_mic_rms, COLOR_OK, LV_PART_INDICATOR);

    s_lbl_mic_rms = make_value_label(tab, "0");
    lv_obj_align(s_lbl_mic_rms, LV_ALIGN_TOP_LEFT, 210, 32);

    lv_obj_t *btn = lv_btn_create(tab);
    lv_obj_set_size(btn, 100, 36);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 65);
    lv_obj_add_event_cb(btn, beep_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Beep");
    lv_obj_center(btn_lbl);

    s_lbl_audio_status = make_value_label(tab, "status: idle");
    lv_obj_set_style_text_color(s_lbl_audio_status, COLOR_MUTED, 0);
    lv_obj_align(s_lbl_audio_status, LV_ALIGN_TOP_LEFT, 0, 110);
}

static void build_ir_tab(lv_obj_t *tab)
{
    lv_obj_t *title = make_section_title(tab, "IR RMT (TX 39 / RX 38)",
                                         COLOR_ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_ir_rx = make_value_label(tab, "RX pulses: 0");
    lv_obj_align(s_lbl_ir_rx, LV_ALIGN_TOP_LEFT, 0, 30);

    s_lbl_ir_last = make_value_label(tab, "last RX: never");
    lv_obj_set_style_text_color(s_lbl_ir_last, COLOR_MUTED, 0);
    lv_obj_align(s_lbl_ir_last, LV_ALIGN_TOP_LEFT, 0, 55);

    lv_obj_t *btn = lv_btn_create(tab);
    lv_obj_set_size(btn, 130, 36);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, 0, 80);
    lv_obj_add_event_cb(btn, ir_send_btn_event, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Send Test");
    lv_obj_center(btn_lbl);

    s_lbl_ir_status = make_value_label(tab, "status: idle");
    lv_obj_set_style_text_color(s_lbl_ir_status, COLOR_MUTED, 0);
    lv_obj_align(s_lbl_ir_status, LV_ALIGN_TOP_LEFT, 0, 125);
}

static void build_buttons_tab(lv_obj_t *tab)
{
    lv_obj_t *title = make_section_title(tab, "Physical buttons", COLOR_ACCENT);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

    static const char *names[3] = {"CONFIG (BOOT)", "MUTE", "MAIN (LCD)"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *name_lbl = lv_label_create(tab);
        lv_label_set_text(name_lbl, names[i]);
        lv_obj_set_style_text_color(name_lbl, COLOR_WARN, 0);
        lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 0, 30 + i * 30);

        s_lbl_btn[i] = make_value_label(tab, "short: 0 / long: 0");
        lv_obj_align(s_lbl_btn[i], LV_ALIGN_TOP_LEFT, 140, 30 + i * 30);
    }
}

void ui_create(const ui_callbacks_t *cbs)
{
    if (cbs) {
        s_cbs = *cbs;
    }

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    s_tabview = lv_tabview_create(scr);
    lv_obj_set_size(s_tabview, 320, 240);
    lv_tabview_set_tab_bar_size(s_tabview, 30);

    lv_obj_t *t_imu      = lv_tabview_add_tab(s_tabview, "IMU");
    lv_obj_t *t_humiture = lv_tabview_add_tab(s_tabview, "Env");
    lv_obj_t *t_radar    = lv_tabview_add_tab(s_tabview, "Radar");
    lv_obj_t *t_audio    = lv_tabview_add_tab(s_tabview, "Audio");
    lv_obj_t *t_ir       = lv_tabview_add_tab(s_tabview, "IR");
    lv_obj_t *t_buttons  = lv_tabview_add_tab(s_tabview, "Btn");

    build_imu_tab(t_imu);
    build_humiture_tab(t_humiture);
    build_radar_tab(t_radar);
    build_audio_tab(t_audio);
    build_ir_tab(t_ir);
    build_buttons_tab(t_buttons);

    s_lbl_uptime = lv_label_create(scr);
    lv_label_set_text(s_lbl_uptime, "0s");
    lv_obj_set_style_text_color(s_lbl_uptime, COLOR_MUTED, 0);
    lv_obj_align(s_lbl_uptime, LV_ALIGN_TOP_RIGHT, -4, 8);

    ESP_LOGI(TAG, "tabview UI created");
}

#define UI_LOCK_MS 50

#define UI_WITH_LOCK(BLOCK)                          \
    do {                                             \
        if (bsp_display_lock(UI_LOCK_MS)) {          \
            BLOCK;                                   \
            bsp_display_unlock();                    \
        }                                            \
    } while (0)

void ui_imu_update(float ax, float ay, float az,
                   float gx, float gy, float gz,
                   float tilt_deg)
{
    UI_WITH_LOCK({
        lv_label_set_text_fmt(s_lbl_acc_x,  "X: %+.2f", ax);
        lv_label_set_text_fmt(s_lbl_acc_y,  "Y: %+.2f", ay);
        lv_label_set_text_fmt(s_lbl_acc_z,  "Z: %+.2f", az);
        lv_label_set_text_fmt(s_lbl_gyro_x, "X: %+6.1f", gx);
        lv_label_set_text_fmt(s_lbl_gyro_y, "Y: %+6.1f", gy);
        lv_label_set_text_fmt(s_lbl_gyro_z, "Z: %+6.1f", gz);
        lv_bar_set_value(s_bar_tilt, (int)tilt_deg, LV_ANIM_ON);
    });
}

void ui_humiture_update(float temp_c, float humidity_pct)
{
    UI_WITH_LOCK({
        lv_label_set_text_fmt(s_lbl_temp, "Temp: %.1f C",   temp_c);
        lv_label_set_text_fmt(s_lbl_hum,  "Hum:  %.1f %%",  humidity_pct);
    });
}

void ui_humiture_set_status(const char *msg)
{
    UI_WITH_LOCK({
        lv_label_set_text(s_lbl_humiture_status, msg);
    });
}

void ui_radar_set_present(bool present, uint32_t event_count)
{
    UI_WITH_LOCK({
        lv_label_set_text(s_lbl_radar_state,
                          present ? "State: PRESENCE" : "State: NO PRESENCE");
        lv_obj_set_style_text_color(s_lbl_radar_state,
                                    present ? COLOR_OK : COLOR_MUTED, 0);
        lv_label_set_text_fmt(s_lbl_radar_count, "Events: %lu",
                              (unsigned long)event_count);
    });
}

void ui_audio_set_mic_rms(int rms)
{
    if (rms < 0) {
        rms = 0;
    }
    UI_WITH_LOCK({
        lv_bar_set_value(s_bar_mic_rms, rms, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_lbl_mic_rms, "%d", rms);
    });
}

void ui_audio_set_status(const char *msg)
{
    UI_WITH_LOCK({
        lv_label_set_text(s_lbl_audio_status, msg);
    });
}

void ui_ir_update(uint32_t rx_pulses, uint32_t last_rx_ms)
{
    UI_WITH_LOCK({
        lv_label_set_text_fmt(s_lbl_ir_rx,   "RX pulses: %lu",
                              (unsigned long)rx_pulses);
        if (last_rx_ms == 0) {
            lv_label_set_text(s_lbl_ir_last, "last RX: never");
        } else {
            lv_label_set_text_fmt(s_lbl_ir_last, "last RX: %lus ago",
                                  (unsigned long)last_rx_ms / 1000);
        }
    });
}

void ui_ir_set_status(const char *msg)
{
    UI_WITH_LOCK({
        lv_label_set_text(s_lbl_ir_status, msg);
    });
}

void ui_buttons_update(int idx, uint32_t short_count, uint32_t long_count)
{
    if (idx < 0 || idx >= 3) {
        return;
    }
    UI_WITH_LOCK({
        lv_label_set_text_fmt(s_lbl_btn[idx], "short: %lu / long: %lu",
                              (unsigned long)short_count,
                              (unsigned long)long_count);
    });
}

void ui_uptime_update(uint64_t seconds)
{
    UI_WITH_LOCK({
        lv_label_set_text_fmt(s_lbl_uptime, "%llus",
                              (unsigned long long)seconds);
    });
}
