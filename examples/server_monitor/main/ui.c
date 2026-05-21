#include "ui.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "bsp/esp-box-3.h"
#include "lvgl.h"

#include "sdkconfig.h"

static const char *TAG = "ui";

#define HOST_NAME_MAX_LEN        32
#define UI_LOCK_MS      50

#define COLOR_ACCENT    lv_color_hex(0x00E5FF)
#define COLOR_OK        lv_color_hex(0x06D6A0)
#define COLOR_WARN      lv_color_hex(0xFFD166)
#define COLOR_MUTED     lv_color_hex(0x9CA3AF)
#define COLOR_BG        lv_color_hex(0x0A0E27)
#define COLOR_BAR_BG    lv_color_hex(0x1F2937)
#define COLOR_PINK      lv_color_hex(0xEF476F)

typedef struct {
    lv_obj_t *tab_content;
    lv_obj_t *dot_status;
    lv_obj_t *lbl_status_word;
    lv_obj_t *lbl_uptime;
    lv_obj_t *bar_cpu;
    lv_obj_t *lbl_cpu_val;
    lv_obj_t *bar_mem;
    lv_obj_t *lbl_mem_val;
    lv_obj_t *bar_gpu;
    lv_obj_t *lbl_gpu_val;
    lv_obj_t *bar_disk;
    lv_obj_t *lbl_disk_val;
    char      host_name[HOST_NAME_MAX_LEN];
} host_ui_t;

static lv_obj_t   *s_tabview;
static lv_obj_t   *s_lbl_status;
static host_ui_t   s_host_ui[CONFIG_BESZEL_MAX_HOSTS];
static int         s_host_ui_count;

#define UI_WITH_LOCK(BLOCK)                          \
    do {                                             \
        if (bsp_display_lock(UI_LOCK_MS)) {          \
            BLOCK;                                   \
            bsp_display_unlock();                    \
        }                                            \
    } while (0)

/* ---------------------- helpers ---------------------- */

static int clamp_pct(int v)
{
    if (v < 0)   return 0;
    if (v > 100) return 100;
    return v;
}

/* Cyan up to 70%, yellow 70-89%, pink at 90%+. Threshold change driven by
 * user preference — flag pressure on any resource (CPU/MEM/GPU/DISK). */
static lv_color_t bar_color_for_pct(int pct)
{
    if (pct >= 90) {
        return COLOR_PINK;
    }
    if (pct >= 70) {
        return COLOR_WARN;
    }
    return COLOR_ACCENT;
}

static void format_uptime(uint32_t seconds, char *buf, size_t cap)
{
    unsigned d = (unsigned)(seconds / 86400);
    unsigned h = (unsigned)((seconds % 86400) / 3600);
    unsigned m = (unsigned)((seconds % 3600) / 60);
    if (d > 0) {
        snprintf(buf, cap, "Up %ud %uh", d, h);
    } else if (h > 0) {
        snprintf(buf, cap, "Up %uh %um", h, m);
    } else {
        snprintf(buf, cap, "Up %um", m);
    }
}

/* ---------------------- per-host tab build ---------------------- */

static void build_metric_row(lv_obj_t *tab, const char *label,
                             int y, lv_obj_t **bar_out, lv_obj_t **val_out)
{
    lv_obj_t *lbl = lv_label_create(tab);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, COLOR_MUTED, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t *bar = lv_bar_create(tab);
    lv_obj_set_size(bar, 200, 14);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 40, y + 2);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_bg_color(bar, COLOR_BAR_BG, 0);
    lv_obj_set_style_bg_color(bar, COLOR_ACCENT, LV_PART_INDICATOR);
    *bar_out = bar;

    lv_obj_t *val = lv_label_create(tab);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, lv_color_black(), 0);
    lv_obj_align(val, LV_ALIGN_TOP_LEFT, 246, y);
    *val_out = val;
}

static void build_host_tab(lv_obj_t *tab, host_ui_t *ui)
{
    memset(ui, 0, sizeof(*ui));
    ui->tab_content = tab;

    ui->dot_status = lv_obj_create(tab);
    lv_obj_set_size(ui->dot_status, 12, 12);
    lv_obj_set_style_radius(ui->dot_status, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ui->dot_status, 0, 0);
    lv_obj_set_style_pad_all(ui->dot_status, 0, 0);
    lv_obj_set_style_bg_color(ui->dot_status, COLOR_MUTED, 0);
    lv_obj_align(ui->dot_status, LV_ALIGN_TOP_LEFT, 0, 4);
    lv_obj_clear_flag(ui->dot_status, LV_OBJ_FLAG_SCROLLABLE);

    ui->lbl_status_word = lv_label_create(tab);
    lv_label_set_text(ui->lbl_status_word, "--");
    lv_obj_set_style_text_color(ui->lbl_status_word, COLOR_MUTED, 0);
    lv_obj_align(ui->lbl_status_word, LV_ALIGN_TOP_LEFT, 20, 0);

    ui->lbl_uptime = lv_label_create(tab);
    lv_label_set_text(ui->lbl_uptime, "");
    lv_obj_set_style_text_color(ui->lbl_uptime, COLOR_MUTED, 0);
    lv_obj_align(ui->lbl_uptime, LV_ALIGN_TOP_RIGHT, 0, 0);

    build_metric_row(tab, "CPU",  30, &ui->bar_cpu,  &ui->lbl_cpu_val);
    build_metric_row(tab, "MEM",  60, &ui->bar_mem,  &ui->lbl_mem_val);
    build_metric_row(tab, "GPU",  90, &ui->bar_gpu,  &ui->lbl_gpu_val);
    build_metric_row(tab, "DISK", 120, &ui->bar_disk, &ui->lbl_disk_val);
}

static void apply_host_data(host_ui_t *ui, const ui_beszel_host_t *h)
{
    int cpu  = clamp_pct(h->cpu_pct);
    int mem  = clamp_pct(h->mem_pct);
    int gpu  = clamp_pct(h->gpu_pct);
    int disk = clamp_pct(h->disk_pct);

    lv_obj_set_style_bg_color(ui->dot_status,
                              h->up ? COLOR_OK : COLOR_PINK, 0);
    lv_label_set_text(ui->lbl_status_word, h->up ? "UP" : "DOWN");
    lv_obj_set_style_text_color(ui->lbl_status_word,
                                h->up ? COLOR_OK : COLOR_PINK, 0);

    char ub[32];
    format_uptime(h->uptime_s, ub, sizeof(ub));
    lv_label_set_text(ui->lbl_uptime, ub);

    lv_obj_set_style_bg_color(ui->bar_cpu, bar_color_for_pct(cpu),
                              LV_PART_INDICATOR);
    lv_bar_set_value(ui->bar_cpu, cpu, LV_ANIM_OFF);
    lv_label_set_text_fmt(ui->lbl_cpu_val, "%d%%", cpu);

    lv_obj_set_style_bg_color(ui->bar_mem, bar_color_for_pct(mem),
                              LV_PART_INDICATOR);
    lv_bar_set_value(ui->bar_mem, mem, LV_ANIM_OFF);
    lv_label_set_text_fmt(ui->lbl_mem_val, "%d%%", mem);

    if (h->gpu_present) {
        lv_obj_set_style_bg_color(ui->bar_gpu, bar_color_for_pct(gpu),
                                  LV_PART_INDICATOR);
        lv_bar_set_value(ui->bar_gpu, gpu, LV_ANIM_OFF);
        lv_label_set_text_fmt(ui->lbl_gpu_val, "%d%%", gpu);
    } else {
        lv_obj_set_style_bg_color(ui->bar_gpu, COLOR_MUTED,
                                  LV_PART_INDICATOR);
        lv_bar_set_value(ui->bar_gpu, 0, LV_ANIM_OFF);
        lv_label_set_text(ui->lbl_gpu_val, "N/A");
    }

    lv_obj_set_style_bg_color(ui->bar_disk, bar_color_for_pct(disk),
                              LV_PART_INDICATOR);
    lv_bar_set_value(ui->bar_disk, disk, LV_ANIM_OFF);
    lv_label_set_text_fmt(ui->lbl_disk_val, "%d%%", disk);
}

/* ---------------------- tabview lifecycle ---------------------- */

static bool topology_changed(const ui_beszel_host_t *hosts, int count)
{
    if (count != s_host_ui_count) {
        return true;
    }
    for (int i = 0; i < count; i++) {
        const char *new_name = hosts[i].name ? hosts[i].name : "";
        if (strncmp(new_name, s_host_ui[i].host_name,
                    sizeof(s_host_ui[i].host_name)) != 0) {
            return true;
        }
    }
    return false;
}

static void create_empty_tabview(void)
{
    lv_obj_t *scr = lv_scr_act();
    s_tabview = lv_tabview_create(scr);
    lv_obj_set_size(s_tabview, 320, 220);
    lv_obj_align(s_tabview, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_tabview_set_tab_bar_size(s_tabview, 30);
}

static void rebuild_tabview(const ui_beszel_host_t *hosts, int count)
{
    if (s_tabview) {
        lv_obj_delete(s_tabview);
        s_tabview = NULL;
    }
    memset(s_host_ui, 0, sizeof(s_host_ui));
    s_host_ui_count = 0;

    create_empty_tabview();

    int n = count;
    if (n > CONFIG_BESZEL_MAX_HOSTS) {
        n = CONFIG_BESZEL_MAX_HOSTS;
    }
    for (int i = 0; i < n; i++) {
        const char *name = hosts[i].name ? hosts[i].name : "?";
        lv_obj_t *tab = lv_tabview_add_tab(s_tabview, name);
        build_host_tab(tab, &s_host_ui[i]);
        strncpy(s_host_ui[i].host_name, name,
                sizeof(s_host_ui[i].host_name) - 1);
    }
    s_host_ui_count = n;

    /* Pull the status footer back above the tabview so it stays clickable
     * (lv_obj_delete + create reset z-order). */
    if (s_lbl_status) {
        lv_obj_move_foreground(s_lbl_status);
    }
}

/* ---------------------- public API ---------------------- */

void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, COLOR_BG, 0);

    create_empty_tabview();

    s_lbl_status = lv_label_create(scr);
    lv_label_set_text(s_lbl_status, "starting...");
    lv_obj_set_style_text_color(s_lbl_status, COLOR_MUTED, 0);
    lv_obj_set_width(s_lbl_status, 320);
    lv_label_set_long_mode(s_lbl_status, LV_LABEL_LONG_DOT);
    lv_obj_align(s_lbl_status, LV_ALIGN_BOTTOM_LEFT, 4, -2);

    ESP_LOGI(TAG, "ui ready");
}

void ui_beszel_replace_hosts(const ui_beszel_host_t *hosts, int count,
                             int active_idx)
{
    if (!hosts || count < 0) {
        return;
    }
    UI_WITH_LOCK({
        if (topology_changed(hosts, count)) {
            rebuild_tabview(hosts, count);
        }
        int n = count;
        if (n > CONFIG_BESZEL_MAX_HOSTS) {
            n = CONFIG_BESZEL_MAX_HOSTS;
        }
        for (int i = 0; i < n; i++) {
            apply_host_data(&s_host_ui[i], &hosts[i]);
        }
        if (n > 0 && s_tabview) {
            int idx = active_idx;
            if (idx < 0) {
                idx = 0;
            } else if (idx >= n) {
                idx = n - 1;
            }
            lv_tabview_set_active(s_tabview, idx, LV_ANIM_OFF);
        }
    });
}

void ui_beszel_select_tab(int idx)
{
    UI_WITH_LOCK({
        if (s_tabview && s_host_ui_count > 0) {
            int i = idx;
            if (i < 0) {
                i = 0;
            } else if (i >= s_host_ui_count) {
                i = s_host_ui_count - 1;
            }
            lv_tabview_set_active(s_tabview, i, LV_ANIM_OFF);
        }
    });
}

void ui_beszel_set_status(const char *msg, uint32_t color_hex)
{
    UI_WITH_LOCK({
        if (s_lbl_status) {
            lv_label_set_text(s_lbl_status, msg ? msg : "");
            lv_obj_set_style_text_color(s_lbl_status,
                                        lv_color_hex(color_hex), 0);
        }
    });
}

void ui_beszel_set_unavailable(const char *reason)
{
    UI_WITH_LOCK({
        if (s_tabview && s_host_ui_count > 0) {
            lv_obj_delete(s_tabview);
            s_tabview = NULL;
            memset(s_host_ui, 0, sizeof(s_host_ui));
            s_host_ui_count = 0;
            create_empty_tabview();
            if (s_lbl_status) {
                lv_obj_move_foreground(s_lbl_status);
            }
        }
        if (s_lbl_status) {
            lv_label_set_text(s_lbl_status,
                              reason ? reason : "unavailable");
            lv_obj_set_style_text_color(s_lbl_status, COLOR_MUTED, 0);
        }
    });
}
