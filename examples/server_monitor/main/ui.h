#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *name;
    bool        up;
    int         cpu_pct;
    int         mem_pct;
    int         gpu_pct;
    bool        gpu_present;
    int         disk_pct;
    uint32_t    uptime_s;
} ui_beszel_host_t;

/**
 * Build the LVGL screen. Must be called inside bsp_display_lock /
 * bsp_display_unlock. After this the screen has an empty tabview plus
 * a status footer, ready for ui_beszel_replace_hosts() to populate.
 */
void ui_create(void);

/**
 * Refresh the host tab list. When the host count or any host name
 * differs from the previous call the tabview is rebuilt; otherwise the
 * existing widgets are just updated in place. `active_idx` is clamped
 * to the new tab count.
 */
void ui_beszel_replace_hosts(const ui_beszel_host_t *hosts, int count,
                             int active_idx);

/**
 * Change which tab is currently showing without touching the data.
 * Out-of-range indices are silently ignored.
 */
void ui_beszel_select_tab(int idx);

/**
 * Update the screen-level status footer. `color_hex` uses 0xRRGGBB so
 * callers do not need to include lvgl.h.
 */
void ui_beszel_set_status(const char *msg, uint32_t color_hex);

/**
 * Helper for "no data yet" / "WiFi missing" / "auth failed" states:
 * empties the tabview, writes `reason` into the footer in muted gray.
 */
void ui_beszel_set_unavailable(const char *reason);

#ifdef __cplusplus
}
#endif
