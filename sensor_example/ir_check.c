#include "ir_check.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_encoder.h"

#include "ui.h"

static const char *TAG = "ir";

#define IR_TX_GPIO         GPIO_NUM_39
#define IR_RX_GPIO         GPIO_NUM_38
#define IR_RESOLUTION_HZ   1000000  /* 1us per tick */
#define IR_CARRIER_FREQ_HZ 38000
#define IR_RX_BUFFER_SYMS  64

static rmt_channel_handle_t s_tx_chan;
static rmt_channel_handle_t s_rx_chan;
static rmt_encoder_handle_t s_copy_encoder;
static QueueHandle_t        s_rx_queue;

static uint32_t s_rx_total_pulses;
static uint32_t s_last_rx_ms;

static rmt_symbol_word_t s_rx_symbols[IR_RX_BUFFER_SYMS];

static const rmt_receive_config_t s_rx_cfg = {
    .signal_range_min_ns = 1250,
    .signal_range_max_ns = 12000000,
};

typedef struct {
    size_t num_symbols;
} ir_rx_evt_t;

static bool IRAM_ATTR rx_done_cb(rmt_channel_handle_t channel,
                                 const rmt_rx_done_event_data_t *edata,
                                 void *user_ctx)
{
    (void)channel;
    BaseType_t hp = pdFALSE;
    ir_rx_evt_t evt = { .num_symbols = edata->num_symbols };
    xQueueSendFromISR(s_rx_queue, &evt, &hp);
    return hp == pdTRUE;
}

static void ir_rx_task(void *arg)
{
    (void)arg;
    rmt_receive(s_rx_chan, s_rx_symbols, sizeof(s_rx_symbols), &s_rx_cfg);

    while (1) {
        ir_rx_evt_t evt;
        if (xQueueReceive(s_rx_queue, &evt, pdMS_TO_TICKS(1000)) == pdTRUE) {
            s_rx_total_pulses += (uint32_t)evt.num_symbols;
            s_last_rx_ms = (uint32_t)(esp_timer_get_time() / 1000);
            ui_ir_update(s_rx_total_pulses, s_last_rx_ms);
            rmt_receive(s_rx_chan, s_rx_symbols, sizeof(s_rx_symbols),
                        &s_rx_cfg);
        } else {
            ui_ir_update(s_rx_total_pulses, s_last_rx_ms);
        }
    }
}

esp_err_t ir_check_init(void)
{
    rmt_tx_channel_config_t tx_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .gpio_num       = IR_TX_GPIO,
    };
    esp_err_t err = rmt_new_tx_channel(&tx_cfg, &s_tx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_tx_channel: %s", esp_err_to_name(err));
        ui_ir_set_status("tx channel fail");
        return err;
    }

    rmt_carrier_config_t carrier = {
        .frequency_hz = IR_CARRIER_FREQ_HZ,
        .duty_cycle   = 0.33f,
    };
    rmt_apply_carrier(s_tx_chan, &carrier);

    rmt_copy_encoder_config_t enc_cfg = {};
    err = rmt_new_copy_encoder(&enc_cfg, &s_copy_encoder);
    if (err != ESP_OK) {
        ui_ir_set_status("encoder fail");
        return err;
    }

    rmt_rx_channel_config_t rx_cfg = {
        .clk_src        = RMT_CLK_SRC_DEFAULT,
        .resolution_hz  = IR_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .gpio_num       = IR_RX_GPIO,
    };
    err = rmt_new_rx_channel(&rx_cfg, &s_rx_chan);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "rmt_new_rx_channel: %s", esp_err_to_name(err));
        ui_ir_set_status("rx channel fail");
        return err;
    }

    s_rx_queue = xQueueCreate(4, sizeof(ir_rx_evt_t));
    rmt_rx_event_callbacks_t rx_cbs = { .on_recv_done = rx_done_cb };
    rmt_rx_register_event_callbacks(s_rx_chan, &rx_cbs, NULL);

    rmt_enable(s_tx_chan);
    rmt_enable(s_rx_chan);

    xTaskCreate(ir_rx_task, "ir_rx", 4096, NULL, 4, NULL);

    ui_ir_set_status("ready (carrier 38kHz)");
    return ESP_OK;
}

void ir_check_send_test(void)
{
    if (!s_tx_chan || !s_copy_encoder) {
        return;
    }
    /* NEC-style header burst + a couple of marker bits; arbitrary but
     * lets a paired RX module record symbols and confirm the path. */
    static const rmt_symbol_word_t pattern[] = {
        {.level0 = 1, .duration0 = 9000, .level1 = 0, .duration1 = 4500},
        {.level0 = 1, .duration0 =  560, .level1 = 0, .duration1 =  560},
        {.level0 = 1, .duration0 =  560, .level1 = 0, .duration1 = 1690},
        {.level0 = 1, .duration0 =  560, .level1 = 0, .duration1 =  560},
        {.level0 = 1, .duration0 =  560, .level1 = 0, .duration1 = 1690},
        {.level0 = 1, .duration0 =  560, .level1 = 0, .duration1 =    0},
    };
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    esp_err_t err = rmt_transmit(s_tx_chan, s_copy_encoder, pattern,
                                 sizeof(pattern), &tx_cfg);
    if (err != ESP_OK) {
        ui_ir_set_status("tx fail");
        return;
    }
    rmt_tx_wait_all_done(s_tx_chan, 200);
    ui_ir_set_status("tx done");
}
