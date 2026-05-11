#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Start the Beszel polling task.
 *
 * Spawns a background task that, once the network is up, authenticates
 * against the configured Beszel server, fetches the list of monitored
 * systems every CONFIG_BESZEL_POLL_INTERVAL_S seconds and pushes the
 * currently-selected host into the UI via ui_beszel_*.
 *
 * Returns ESP_OK after the task is created, regardless of whether the
 * first poll has happened yet.
 */
esp_err_t beszel_init(void);

/* Advance / retreat the on-screen host selection. Both are safe to call
 * from button callbacks; they take an internal mutex and immediately
 * republish to the UI so the user sees the new host without waiting for
 * the next poll cycle. */
void      beszel_select_prev(void);
void      beszel_select_next(void);

#ifdef __cplusplus
}
#endif
