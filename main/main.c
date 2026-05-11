#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"

#include "bsp/esp-box-3.h"
#include "lvgl.h"
#include "icm42670.h"

static const char *TAG = "BOX3_SENSOR";

// ===== IMU 핸들 =====
static icm42670_handle_t imu = NULL;

// ===== LVGL 위젯 핸들 =====
static lv_obj_t *label_title;
static lv_obj_t *label_accel_x;
static lv_obj_t *label_accel_y;
static lv_obj_t *label_accel_z;
static lv_obj_t *label_gyro_x;
static lv_obj_t *label_gyro_y;
static lv_obj_t *label_gyro_z;
static lv_obj_t *label_tilt;
static lv_obj_t *bar_tilt;
static lv_obj_t *label_uptime;

// ===== IMU 초기화 =====
static esp_err_t imu_init(void)
{
    // BSP가 이미 I2C를 켜놓은 핸들을 가져옴
    i2c_master_bus_handle_t i2c_handle = bsp_i2c_get_handle();

    ESP_ERROR_CHECK(icm42670_create(i2c_handle, ICM42670_I2C_ADDRESS, &imu));

    icm42670_cfg_t imu_cfg = {
        .acce_fs  = ACCE_FS_4G,
        .acce_odr = ACCE_ODR_400HZ,
        .gyro_fs  = GYRO_FS_2000DPS,
        .gyro_odr = GYRO_ODR_400HZ,
    };
    ESP_ERROR_CHECK(icm42670_config(imu, &imu_cfg));

    ESP_ERROR_CHECK(icm42670_acce_set_pwr(imu, ACCE_PWR_LOWNOISE));
    ESP_ERROR_CHECK(icm42670_gyro_set_pwr(imu, GYRO_PWR_LOWNOISE));

    ESP_LOGI(TAG, "IMU 초기화 완료");
    return ESP_OK;
}

// ===== LVGL UI 구성 =====
static void create_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0E27), 0);
    lv_obj_set_style_pad_all(scr, 8, 0);

    // --- 타이틀 ---
    label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "ESP32-S3-BOX-3 IMU");
    lv_obj_set_style_text_color(label_title, lv_color_hex(0x00E5FF), 0);
    lv_obj_set_style_text_font(label_title, &lv_font_montserrat_14, 0);
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 0);

    // --- 가속도 섹션 ---
    lv_obj_t *lbl_acc_title = lv_label_create(scr);
    lv_label_set_text(lbl_acc_title, "Accel (g)");
    lv_obj_set_style_text_color(lbl_acc_title, lv_color_hex(0xFFD166), 0);
    lv_obj_set_style_text_font(lbl_acc_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_acc_title, LV_ALIGN_TOP_LEFT, 5, 30);

    label_accel_x = lv_label_create(scr);
    label_accel_y = lv_label_create(scr);
    label_accel_z = lv_label_create(scr);
    lv_obj_set_style_text_color(label_accel_x, lv_color_white(), 0);
    lv_obj_set_style_text_color(label_accel_y, lv_color_white(), 0);
    lv_obj_set_style_text_color(label_accel_z, lv_color_white(), 0);
    lv_obj_align(label_accel_x, LV_ALIGN_TOP_LEFT, 5, 50);
    lv_obj_align(label_accel_y, LV_ALIGN_TOP_LEFT, 5, 70);
    lv_obj_align(label_accel_z, LV_ALIGN_TOP_LEFT, 5, 90);
    lv_label_set_text(label_accel_x, "X: ---");
    lv_label_set_text(label_accel_y, "Y: ---");
    lv_label_set_text(label_accel_z, "Z: ---");

    // --- 자이로 섹션 ---
    lv_obj_t *lbl_gyro_title = lv_label_create(scr);
    lv_label_set_text(lbl_gyro_title, "Gyro (dps)");
    lv_obj_set_style_text_color(lbl_gyro_title, lv_color_hex(0xEF476F), 0);
    lv_obj_set_style_text_font(lbl_gyro_title, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl_gyro_title, LV_ALIGN_TOP_LEFT, 165, 30);

    label_gyro_x = lv_label_create(scr);
    label_gyro_y = lv_label_create(scr);
    label_gyro_z = lv_label_create(scr);
    lv_obj_set_style_text_color(label_gyro_x, lv_color_white(), 0);
    lv_obj_set_style_text_color(label_gyro_y, lv_color_white(), 0);
    lv_obj_set_style_text_color(label_gyro_z, lv_color_white(), 0);
    lv_obj_align(label_gyro_x, LV_ALIGN_TOP_LEFT, 165, 50);
    lv_obj_align(label_gyro_y, LV_ALIGN_TOP_LEFT, 165, 70);
    lv_obj_align(label_gyro_z, LV_ALIGN_TOP_LEFT, 165, 90);
    lv_label_set_text(label_gyro_x, "X: ---");
    lv_label_set_text(label_gyro_y, "Y: ---");
    lv_label_set_text(label_gyro_z, "Z: ---");

    // --- 기울기 막대 ---
    label_tilt = lv_label_create(scr);
    lv_label_set_text(label_tilt, "Tilt");
    lv_obj_set_style_text_color(label_tilt, lv_color_hex(0x06D6A0), 0);
    lv_obj_align(label_tilt, LV_ALIGN_TOP_LEFT, 5, 130);

    bar_tilt = lv_bar_create(scr);
    lv_obj_set_size(bar_tilt, 300, 25);
    lv_obj_align(bar_tilt, LV_ALIGN_TOP_MID, 0, 155);
    lv_bar_set_range(bar_tilt, 0, 90);
    lv_obj_set_style_bg_color(bar_tilt, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_color(bar_tilt, lv_color_hex(0x06D6A0), LV_PART_INDICATOR);

    // --- 업타임 ---
    label_uptime = lv_label_create(scr);
    lv_obj_set_style_text_color(label_uptime, lv_color_hex(0x9CA3AF), 0);
    lv_obj_set_style_text_font(label_uptime, &lv_font_montserrat_14, 0);
    lv_obj_align(label_uptime, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_label_set_text(label_uptime, "uptime: 0s");
}

// ===== 센서 읽기 + UI 업데이트 태스크 =====
static void sensor_task(void *arg)
{
    icm42670_value_t accel = {0};
    icm42670_value_t gyro  = {0};

    while (1) {
        // 센서 값 읽기
        if (icm42670_get_acce_value(imu, &accel) == ESP_OK &&
            icm42670_get_gyro_value(imu, &gyro)  == ESP_OK) {

            // 기울기 각도 계산 (Z축 기준, 0~90도)
            float tilt = atan2f(sqrtf(accel.x * accel.x + accel.y * accel.y),
                                fabsf(accel.z)) * 180.0f / M_PI;

            // 시리얼 로그
            ESP_LOGI(TAG, "Accel[%.2f, %.2f, %.2f] Gyro[%.1f, %.1f, %.1f] Tilt=%.1f",
                     accel.x, accel.y, accel.z,
                     gyro.x, gyro.y, gyro.z, tilt);

            // LVGL 락 잡고 UI 업데이트 (반드시 락 필요)
            if (bsp_display_lock(100)) {
                lv_label_set_text_fmt(label_accel_x, "X: %+.2f", accel.x);
                lv_label_set_text_fmt(label_accel_y, "Y: %+.2f", accel.y);
                lv_label_set_text_fmt(label_accel_z, "Z: %+.2f", accel.z);

                lv_label_set_text_fmt(label_gyro_x, "X: %+6.1f", gyro.x);
                lv_label_set_text_fmt(label_gyro_y, "Y: %+6.1f", gyro.y);
                lv_label_set_text_fmt(label_gyro_z, "Z: %+6.1f", gyro.z);

                lv_bar_set_value(bar_tilt, (int)tilt, LV_ANIM_ON);

                lv_label_set_text_fmt(label_uptime, "uptime: %llus",
                                      esp_timer_get_time() / 1000000);
                bsp_display_unlock();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100));  // 10Hz 갱신
    }
}

// ===== 진입점 =====
void app_main(void)
{
    ESP_LOGI(TAG, "BOX-3 센서 데모 시작");

    // 1. BSP가 I2C, 디스플레이, 터치, LVGL 한 번에 초기화
    bsp_i2c_init();
    bsp_display_start();
    bsp_display_backlight_on();

    // 2. IMU 초기화
    ESP_ERROR_CHECK(imu_init());

    // 3. UI 만들기 (LVGL 락 안에서)
    bsp_display_lock(0);
    create_ui();
    bsp_display_unlock();

    // 4. 센서 갱신 태스크 시작
    xTaskCreatePinnedToCore(sensor_task, "sensor", 4096, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "초기화 완료, 메인 루프 진입");
}