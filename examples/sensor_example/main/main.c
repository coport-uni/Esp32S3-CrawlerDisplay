#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "bsp/esp-box-3.h"

#include "ui.h"
#include "sensors.h"
#include "audio_check.h"
#include "ir_check.h"
#include "buttons_check.h"

static const char *TAG = "main";

static void uptime_task(void *arg)
{
    (void)arg;
    while (1) {
        uint64_t seconds = (uint64_t)(esp_timer_get_time() / 1000000ULL);
        ui_uptime_update(seconds);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void on_beep_pressed(void)
{
    audio_check_beep();
}

static void on_ir_send_pressed(void)
{
    ir_check_send_test();
}

void app_main(void)
{
    ESP_LOGI(TAG, "BOX-3 + SENSOR self-test starting");

    ESP_ERROR_CHECK(bsp_i2c_init());
    bsp_display_start();
    bsp_display_backlight_on();

    bsp_display_lock(0);
    ui_callbacks_t cbs = {
        .on_beep    = on_beep_pressed,
        .on_ir_send = on_ir_send_pressed,
    };
    ui_create(&cbs);
    bsp_display_unlock();

    sensors_init();
    audio_check_init();
    ir_check_init();
    buttons_check_init();

    xTaskCreatePinnedToCore(uptime_task, "uptime", 2048, NULL, 2, NULL, 0);

    ESP_LOGI(TAG, "init complete");
}
