#include "buttons_check.h"

#include <stdatomic.h>
#include <stdint.h>

#include "esp_log.h"
#include "iot_button.h"
#include "bsp/esp-box-3.h"

#include "ui.h"

static const char *TAG = "buttons";

typedef struct {
    atomic_uint short_count;
    atomic_uint long_count;
    int index;
} btn_state_t;

static btn_state_t s_state[BSP_BUTTON_NUM];

static void on_short(void *handle, void *usr_data)
{
    (void)handle;
    btn_state_t *st = (btn_state_t *)usr_data;
    unsigned s = atomic_fetch_add(&st->short_count, 1) + 1;
    unsigned l = atomic_load(&st->long_count);
    ui_buttons_update(st->index, s, l);
}

static void on_long(void *handle, void *usr_data)
{
    (void)handle;
    btn_state_t *st = (btn_state_t *)usr_data;
    unsigned l = atomic_fetch_add(&st->long_count, 1) + 1;
    unsigned s = atomic_load(&st->short_count);
    ui_buttons_update(st->index, s, l);
}

esp_err_t buttons_check_init(void)
{
    button_handle_t handles[BSP_BUTTON_NUM] = {0};
    int btn_cnt = 0;
    esp_err_t err = bsp_iot_button_create(handles, &btn_cnt, BSP_BUTTON_NUM);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_iot_button_create: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "registered %d buttons", btn_cnt);

    for (int i = 0; i < btn_cnt && i < BSP_BUTTON_NUM; i++) {
        s_state[i].index = i;
        atomic_store(&s_state[i].short_count, 0);
        atomic_store(&s_state[i].long_count, 0);
        if (!handles[i]) {
            continue;
        }
        iot_button_register_cb(handles[i], BUTTON_SINGLE_CLICK,
                               NULL, on_short, &s_state[i]);
        iot_button_register_cb(handles[i], BUTTON_LONG_PRESS_START,
                               NULL, on_long, &s_state[i]);
        ui_buttons_update(i, 0, 0);
    }
    return ESP_OK;
}
