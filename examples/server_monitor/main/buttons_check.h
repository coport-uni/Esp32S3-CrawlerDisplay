#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*on_config)(void);   /* short press on BSP_BUTTON_CONFIG */
    void (*on_mute)(void);     /* short press on BSP_BUTTON_MUTE   */
} buttons_callbacks_t;

esp_err_t buttons_check_init(const buttons_callbacks_t *cbs);

#ifdef __cplusplus
}
#endif
