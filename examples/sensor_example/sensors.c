#include "sensors.h"

#include <math.h>
#include <stdatomic.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "bsp/esp-box-3.h"
#include "icm42670.h"
#include "iot_sensor_hub.h"

#include "ui.h"

static const char *TAG = "sensors";

#define RADAR_INT_GPIO   GPIO_NUM_21
#define IMU_TASK_PERIOD_MS  100
#define HUMITURE_PERIOD_MS  1000

static icm42670_handle_t s_imu;
static sensor_handle_t   s_humiture;

static TaskHandle_t s_radar_task_handle;
static atomic_uint  s_radar_event_count;
static atomic_int   s_radar_present_flag;

static esp_err_t imu_setup(void)
{
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    esp_err_t err = icm42670_create(bus, ICM42670_I2C_ADDRESS, &s_imu);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "icm42670_create: %s", esp_err_to_name(err));
        return err;
    }

    icm42670_cfg_t cfg = {
        .acce_fs  = ACCE_FS_4G,
        .acce_odr = ACCE_ODR_400HZ,
        .gyro_fs  = GYRO_FS_2000DPS,
        .gyro_odr = GYRO_ODR_400HZ,
    };
    ESP_ERROR_CHECK(icm42670_config(s_imu, &cfg));
    ESP_ERROR_CHECK(icm42670_acce_set_pwr(s_imu, ACCE_PWR_LOWNOISE));
    ESP_ERROR_CHECK(icm42670_gyro_set_pwr(s_imu, GYRO_PWR_LOWNOISE));
    return ESP_OK;
}

static void imu_task(void *arg)
{
    (void)arg;
    icm42670_value_t accel = {0};
    icm42670_value_t gyro  = {0};

    while (1) {
        if (icm42670_get_acce_value(s_imu, &accel) == ESP_OK &&
            icm42670_get_gyro_value(s_imu, &gyro)  == ESP_OK) {

            float tilt = atan2f(sqrtf(accel.x * accel.x + accel.y * accel.y),
                                fabsf(accel.z)) * 180.0f / (float)M_PI;

            ui_imu_update(accel.x, accel.y, accel.z,
                          gyro.x, gyro.y, gyro.z, tilt);
        }
        vTaskDelay(pdMS_TO_TICKS(IMU_TASK_PERIOD_MS));
    }
}

static void humiture_event_cb(void *arg, sensor_event_base_t base,
                              int32_t event_id, void *event_data)
{
    (void)arg;
    (void)base;
    sensor_data_t *d = (sensor_data_t *)event_data;
    if (!d) {
        return;
    }
    static float last_temp = 0.0f;
    static float last_hum  = 0.0f;
    if (event_id == SENSOR_TEMP_DATA_READY) {
        last_temp = d->temperature;
    } else if (event_id == SENSOR_HUMI_DATA_READY) {
        last_hum = d->humidity;
    } else {
        return;
    }
    ui_humiture_update(last_temp, last_hum);
}

static esp_err_t humiture_setup(void)
{
    bsp_sensor_config_t cfg = {
        .type   = HUMITURE_ID,
        .mode   = MODE_POLLING,
        .period = HUMITURE_PERIOD_MS,
    };
    esp_err_t err = bsp_sensor_init(&cfg, &s_humiture);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bsp_sensor_init(HUMITURE): %s", esp_err_to_name(err));
        ui_humiture_set_status("init failed");
        return err;
    }

    iot_sensor_handler_register(s_humiture, humiture_event_cb, NULL);

    err = iot_sensor_start(s_humiture);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "iot_sensor_start: %s", esp_err_to_name(err));
        ui_humiture_set_status("start failed");
        return err;
    }
    ui_humiture_set_status("polling 1s");
    return ESP_OK;
}

static void IRAM_ATTR radar_isr(void *arg)
{
    (void)arg;
    BaseType_t hp = pdFALSE;
    vTaskNotifyGiveFromISR(s_radar_task_handle, &hp);
    portYIELD_FROM_ISR(hp);
}

static void radar_task(void *arg)
{
    (void)arg;
    const TickType_t debounce = pdMS_TO_TICKS(150);
    while (1) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500)) > 0) {
            atomic_fetch_add(&s_radar_event_count, 1);
            atomic_store(&s_radar_present_flag, 1);
            ui_radar_set_present(true,
                                 atomic_load(&s_radar_event_count));
            vTaskDelay(debounce);
        } else {
            int level = gpio_get_level(RADAR_INT_GPIO);
            int prev  = atomic_load(&s_radar_present_flag);
            if (level == 0 && prev != 0) {
                atomic_store(&s_radar_present_flag, 0);
                ui_radar_set_present(false,
                                     atomic_load(&s_radar_event_count));
            }
        }
    }
}

static esp_err_t radar_setup(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << RADAR_INT_GPIO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    BaseType_t ok = xTaskCreatePinnedToCore(radar_task, "radar", 3072,
                                            NULL, 4, &s_radar_task_handle, 1);
    if (ok != pdPASS) {
        return ESP_FAIL;
    }

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    ESP_ERROR_CHECK(gpio_isr_handler_add(RADAR_INT_GPIO, radar_isr, NULL));
    return ESP_OK;
}

esp_err_t sensors_init(void)
{
    atomic_store(&s_radar_event_count, 0);
    atomic_store(&s_radar_present_flag, 0);

    ESP_ERROR_CHECK(imu_setup());
    xTaskCreatePinnedToCore(imu_task, "imu", 4096, NULL, 5, NULL, 1);

    humiture_setup();
    radar_setup();
    return ESP_OK;
}
