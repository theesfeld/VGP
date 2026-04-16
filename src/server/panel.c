#include "panel.h"
#include "server.h"
#include "window.h"
#include "vgp/log.h"
#include "vgp/protocol.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

#define TAG "panel"

/* ============================================================
 * Geometry
 * ============================================================ */

vgp_panel_geom_t vgp_panel_geometry(const vgp_config_panel_t *cfg,
                                      const vgp_theme_t *theme,
                                      uint32_t output_w, uint32_t output_h)
{
    float h = (cfg->height > 0) ? (float)cfg->height : theme->statusbar_height;
    bool top = (strcmp(cfg->position, "top") == 0);
    return (vgp_panel_geom_t){
        .x = 0,
        .y = top ? 0.0f : (float)output_h - h,
        .w = (float)output_w,
        .h = h,
        .top = top,
    };
}

float vgp_panel_usable_height(const vgp_config_panel_t *cfg,
                                const vgp_theme_t *theme,
                                uint32_t output_h)
{
    float h = (cfg->height > 0) ? (float)cfg->height : theme->statusbar_height;
    return (float)output_h - h;
}

/* ============================================================
 * Widget rendering (internal)
 * ============================================================ */

typedef struct {
    vgp_render_backend_t *b;
    void                 *ctx;
    const vgp_theme_t    *theme;
    vgp_compositor_t     *comp;
    int                   workspace;
    float                 bar_y, bar_h, fs, text_y, pad;
    uint32_t              width, height;
    const vgp_color_t    *bg, *ac, *tc;
} panel_ctx_t;

static float widget_workspaces(panel_ctx_t *p, float x)
{
    int ws_count = 9;
    for (int ws = 0; ws < ws_count; ws++) {
        bool has_windows = false;
        for (int i = 0; i < p->comp->window_count; i++) {
            if (p->comp->z_order[i]->workspace == ws && p->comp->z_order[i]->visible) {
                has_windows = true;
                break;
            }
        }

        bool is_active = (ws == p->workspace);
        float btn_w = 22.0f;
        float btn_h = p->bar_h - 8.0f;
        float btn_y = p->bar_y + 4.0f;

        if (is_active) {
            p->b->ops->draw_rounded_rect(p->b, p->ctx, x, btn_y, btn_w, btn_h, 3.0f,
                                           p->ac->r, p->ac->g, p->ac->b, 0.8f);
            char num[4]; snprintf(num, sizeof(num), "%d", ws + 1);
            p->b->ops->draw_text(p->b, p->ctx, num, -1, x + 7, p->text_y, p->fs,
                                   0.0f, 0.0f, 0.0f, 1.0f);
        } else if (has_windows) {
            p->b->ops->draw_rounded_rect(p->b, p->ctx, x, btn_y, btn_w, btn_h, 3.0f,
                                           p->tc->r * 0.3f, p->tc->g * 0.3f, p->tc->b * 0.3f, 0.5f);
            char num[4]; snprintf(num, sizeof(num), "%d", ws + 1);
            p->b->ops->draw_text(p->b, p->ctx, num, -1, x + 7, p->text_y, p->fs,
                                   p->tc->r, p->tc->g, p->tc->b, 0.8f);
        } else {
            char num[4]; snprintf(num, sizeof(num), "%d", ws + 1);
            p->b->ops->draw_text(p->b, p->ctx, num, -1, x + 7, p->text_y, p->fs,
                                   p->tc->r * 0.4f, p->tc->g * 0.4f, p->tc->b * 0.4f, 0.4f);
        }
        x += btn_w + 2.0f;
    }
    return x;
}

static float widget_taskbar(panel_ctx_t *p, float x, float max_w)
{
    float taskbar_start = x;
    int win_count = 0;
    for (int i = 0; i < p->comp->window_count; i++) {
        vgp_window_t *w = p->comp->z_order[i];
        if (w->visible && w->workspace == p->workspace && w->decorated)
            win_count++;
    }

    if (win_count > 0 && max_w > 0) {
        float entry_w = max_w / (float)win_count;
        if (entry_w > 250.0f) entry_w = 250.0f;
        float ex = taskbar_start;

        for (int i = 0; i < p->comp->window_count; i++) {
            vgp_window_t *w = p->comp->z_order[i];
            if (!w->visible || w->workspace != p->workspace || !w->decorated) continue;

            bool is_focused = (w == p->comp->focused);
            float ew = entry_w - 4.0f;
            float eh = p->bar_h - 8.0f;
            float ey = p->bar_y + 4.0f;

            int32_t out_offset = 0;
            for (int oi = 0; oi < p->comp->output_count; oi++) {
                if (p->comp->outputs[oi].workspace == p->workspace) {
                    out_offset = p->comp->outputs[oi].x; break;
                }
            }
            float local_mx = p->comp->cursor.x - (float)out_offset;
            float local_my = p->comp->cursor.y;
            bool is_hover = (local_my >= ey && local_my < ey + eh &&
                              local_mx >= ex + 2 && local_mx < ex + 2 + ew);

            if (is_focused) {
                p->b->ops->draw_rounded_rect(p->b, p->ctx, ex + 2, ey, ew, eh, 4.0f,
                    p->ac->r * 0.3f, p->ac->g * 0.3f, p->ac->b * 0.3f, 0.6f);
                p->b->ops->draw_rounded_rect(p->b, p->ctx, ex + 6, ey + eh - 3, ew - 8, 2.0f, 1.0f,
                    p->ac->r, p->ac->g, p->ac->b, 1.0f);
            } else if (is_hover) {
                p->b->ops->draw_rounded_rect(p->b, p->ctx, ex + 2, ey, ew, eh, 4.0f,
                    p->ac->r * 0.15f, p->ac->g * 0.15f, p->ac->b * 0.15f, 0.4f);
            } else {
                p->b->ops->draw_rounded_rect(p->b, p->ctx, ex + 2, ey, ew, eh, 4.0f,
                    p->tc->r * 0.1f, p->tc->g * 0.1f, p->tc->b * 0.1f, 0.3f);
            }

            float max_text_w = ew - 12.0f;
            int max_chars = (int)(max_text_w / (p->fs * 0.55f));
            if (max_chars > 0) {
                char truncated[64];
                int title_len = (int)strlen(w->title);
                if (title_len > max_chars && max_chars > 3)
                    snprintf(truncated, sizeof(truncated), "%.*s...", max_chars - 3, w->title);
                else
                    snprintf(truncated, sizeof(truncated), "%.*s", max_chars, w->title);
                p->b->ops->draw_text(p->b, p->ctx, truncated, -1,
                    ex + 8, p->text_y, p->fs - 1,
                    p->tc->r, p->tc->g, p->tc->b, is_focused ? 1.0f : 0.6f);
            }
            ex += entry_w;
        }
    }
    return taskbar_start + max_w;
}

static float widget_clock(panel_ctx_t *p, float x)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    p->b->ops->draw_text(p->b, p->ctx, buf, -1, x, p->text_y, p->fs,
                           p->tc->r, p->tc->g, p->tc->b, p->tc->a);
    return x + 50.0f;
}

static float widget_date(panel_ctx_t *p, float x)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[32]; snprintf(buf, sizeof(buf), "%02d/%02d", t->tm_mon + 1, t->tm_mday);
    p->b->ops->draw_text(p->b, p->ctx, buf, -1, x, p->text_y, p->fs - 2,
                           p->tc->r * 0.7f, p->tc->g * 0.7f, p->tc->b * 0.7f, 0.7f);
    return x + 50.0f;
}

static float widget_cpu(panel_ctx_t *p, float x)
{
    static long prev_idle = 0, prev_total = 0;
    static int cpu_pct = 0;
    FILE *f = fopen("/proc/stat", "r");
    if (f) {
        long user, nice, sys, idle, iow, irq, sirq, steal;
        if (fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
                   &user, &nice, &sys, &idle, &iow, &irq, &sirq, &steal) == 8) {
            long total = user + nice + sys + idle + iow + irq + sirq + steal;
            long di = idle - prev_idle, dt = total - prev_total;
            if (dt > 0) cpu_pct = (int)(100 * (dt - di) / dt);
            prev_idle = idle; prev_total = total;
        }
        fclose(f);
    }
    char buf[16]; snprintf(buf, sizeof(buf), "CPU %d%%", cpu_pct);
    p->b->ops->draw_text(p->b, p->ctx, buf, -1, x, p->text_y, p->fs - 1,
                           p->tc->r, p->tc->g, p->tc->b, 0.7f);
    return x + 60.0f;
}

static float widget_memory(panel_ctx_t *p, float x)
{
    long total = 0, avail = 0;
    FILE *f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &total);
            else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &avail);
        }
        fclose(f);
    }
    int pct = total > 0 ? (int)(100 * (total - avail) / total) : 0;
    char buf[16]; snprintf(buf, sizeof(buf), "MEM %d%%", pct);
    p->b->ops->draw_text(p->b, p->ctx, buf, -1, x, p->text_y, p->fs - 1,
                           p->tc->r, p->tc->g, p->tc->b, 0.7f);
    return x + 70.0f;
}

static float widget_battery(panel_ctx_t *p, float x)
{
    int cap = -1; char st[32] = "";
    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (f) { fscanf(f, "%d", &cap); fclose(f); }
    f = fopen("/sys/class/power_supply/BAT0/status", "r");
    if (f) { fscanf(f, "%31s", st); fclose(f); }
    if (cap >= 0) {
        char buf[32]; snprintf(buf, sizeof(buf), "BAT %s%d%%",
                                strcmp(st, "Charging") == 0 ? "+" : "", cap);
        p->b->ops->draw_text(p->b, p->ctx, buf, -1, x, p->text_y, p->fs - 1,
                               p->tc->r, p->tc->g, p->tc->b, 0.7f);
        return x + 70.0f;
    }
    return x;
}

static float widget_volume(panel_ctx_t *p, float x)
{
    static int vol_pct = -1;
    static bool muted = false;
    static int poll_counter = 0;
    /* Poll volume every ~60 frames (~1 second) */
    if (poll_counter++ >= 60 || vol_pct < 0) {
        poll_counter = 0;
        FILE *f = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@ 2>/dev/null", "r");
        if (f) {
            char line[128];
            if (fgets(line, sizeof(line), f)) {
                /* Format: "Volume: 0.75" or "Volume: 0.75 [MUTED]" */
                float v = 0;
                if (sscanf(line, "Volume: %f", &v) == 1)
                    vol_pct = (int)(v * 100);
                muted = (strstr(line, "MUTED") != NULL);
            }
            pclose(f);
        }
    }
    char buf[16];
    if (vol_pct >= 0) {
        if (muted)
            snprintf(buf, sizeof(buf), "VOL MUTE");
        else
            snprintf(buf, sizeof(buf), "VOL %d%%", vol_pct);
    } else {
        snprintf(buf, sizeof(buf), "VOL --");
    }
    p->b->ops->draw_text(p->b, p->ctx, buf, -1, x, p->text_y, p->fs - 1,
                           p->tc->r, p->tc->g, p->tc->b, 0.7f);
    return x + 70.0f;
}

static float widget_network(panel_ctx_t *p, float x)
{
    static char net_status[32] = "NET --";
    static int poll_counter = 0;
    /* Poll every ~120 frames (~2 seconds) */
    if (poll_counter++ >= 120) {
        poll_counter = 0;
        /* Check /sys/class/net for an active interface */
        const char *ifaces[] = {"wlan0", "wlp", "eth0", "enp", NULL};
        bool found = false;
        FILE *f;
        char path[128], state[32];
        /* Try common wireless interfaces first */
        for (int i = 0; !found && ifaces[i]; i++) {
            /* Check if interface exists and is up */
            snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", ifaces[i]);
            f = fopen(path, "r");
            if (f) {
                if (fscanf(f, "%31s", state) == 1 && strcmp(state, "up") == 0) {
                    snprintf(net_status, sizeof(net_status), "NET %s", ifaces[i]);
                    found = true;
                }
                fclose(f);
            }
        }
        if (!found) {
            /* Scan /sys/class/net for any up interface */
            f = popen("for d in /sys/class/net/*/operstate; do "
                      "iface=$(echo $d | cut -d/ -f5); "
                      "[ \"$(cat $d 2>/dev/null)\" = up ] && [ \"$iface\" != lo ] && "
                      "echo $iface && break; done 2>/dev/null", "r");
            if (f) {
                char iface[32];
                if (fgets(iface, sizeof(iface), f)) {
                    size_t len = strlen(iface);
                    while (len > 0 && iface[len-1] == '\n') iface[--len] = '\0';
                    if (iface[0])
                        snprintf(net_status, sizeof(net_status), "NET %s", iface);
                    else
                        snprintf(net_status, sizeof(net_status), "NET OFF");
                    found = true;
                }
                pclose(f);
            }
            if (!found)
                snprintf(net_status, sizeof(net_status), "NET OFF");
        }
    }
    p->b->ops->draw_text(p->b, p->ctx, net_status, -1, x, p->text_y, p->fs - 1,
                           p->tc->r, p->tc->g, p->tc->b, 0.7f);
    return x + 80.0f;
}

static float render_widget(panel_ctx_t *p, const char *name, float x, float max_w)
{
    if (strcmp(name, "workspaces") == 0) return widget_workspaces(p, x);
    if (strcmp(name, "taskbar") == 0)    return widget_taskbar(p, x, max_w);
    if (strcmp(name, "clock") == 0)      return widget_clock(p, x);
    if (strcmp(name, "date") == 0)       return widget_date(p, x);
    if (strcmp(name, "cpu") == 0)        return widget_cpu(p, x);
    if (strcmp(name, "memory") == 0)     return widget_memory(p, x);
    if (strcmp(name, "battery") == 0)    return widget_battery(p, x);
    if (strcmp(name, "volume") == 0)     return widget_volume(p, x);
    if (strcmp(name, "network") == 0)    return widget_network(p, x);
    return x;
}

/* ============================================================
 * Panel rendering
 * ============================================================ */

void vgp_panel_render(vgp_render_backend_t *b, void *ctx,
                       const vgp_theme_t *theme,
                       const vgp_config_panel_t *cfg,
                       uint32_t output_w, uint32_t output_h,
                       int workspace,
                       vgp_compositor_t *comp)
{
    vgp_panel_geom_t g = vgp_panel_geometry(cfg, theme, output_w, output_h);
    float fs = theme->statusbar_font_size;
    float text_y = g.y + g.h / 2.0f + fs / 3.0f;
    float pad = 6.0f;

    const vgp_color_t *bg = &theme->statusbar_bg;
    const vgp_color_t *ac = &theme->border_active;
    const vgp_color_t *tc = &theme->statusbar_text;

    /* Background */
    b->ops->draw_rect(b, ctx, g.x, g.y, g.w, g.h, bg->r, bg->g, bg->b, bg->a);

    /* Border line (on the edge facing the desktop) */
    float border_y = g.top ? g.y + g.h : g.y;
    b->ops->draw_line(b, ctx, 0, border_y, g.w, border_y, 1.0f,
                       ac->r, ac->g, ac->b, 0.5f);

    panel_ctx_t p = {
        .b = b, .ctx = ctx, .theme = theme, .comp = comp,
        .workspace = workspace, .bar_y = g.y, .bar_h = g.h,
        .fs = fs, .text_y = text_y, .pad = pad,
        .width = output_w, .height = output_h,
        .bg = bg, .ac = ac, .tc = tc,
    };

    /* LEFT widgets */
    float x_left = pad;
    for (int i = 0; i < cfg->left_count; i++) {
        x_left = render_widget(&p, cfg->left_widgets[i], x_left, 0);
        if (i < cfg->left_count - 1) {
            x_left += pad;
            b->ops->draw_line(b, ctx, x_left, g.y + 6, x_left, g.y + g.h - 6, 1.0f,
                               tc->r, tc->g, tc->b, 0.2f);
            x_left += pad * 2;
        }
    }
    x_left += pad;

    /* RIGHT: estimate widths then render */
    float right_total = 0;
    for (int i = 0; i < cfg->right_count; i++) {
        const char *name = cfg->right_widgets[i];
        if (strcmp(name, "clock") == 0) right_total += 50.0f;
        else if (strcmp(name, "date") == 0) right_total += 50.0f;
        else if (strcmp(name, "cpu") == 0) right_total += 60.0f;
        else if (strcmp(name, "memory") == 0) right_total += 70.0f;
        else if (strcmp(name, "battery") == 0) right_total += 70.0f;
        else if (strcmp(name, "volume") == 0) right_total += 70.0f;
        else if (strcmp(name, "network") == 0) right_total += 80.0f;
        else right_total += 50.0f;
        if (i < cfg->right_count - 1) right_total += pad * 3;
    }
    float right_start = g.w - pad - right_total;

    if (cfg->right_count > 0 && cfg->left_count + cfg->center_count > 0) {
        b->ops->draw_line(b, ctx, right_start - pad, g.y + 6,
                           right_start - pad, g.y + g.h - 6, 1.0f,
                           tc->r, tc->g, tc->b, 0.2f);
    }

    float xr = right_start;
    for (int i = 0; i < cfg->right_count; i++) {
        xr = render_widget(&p, cfg->right_widgets[i], xr, 0);
        if (i < cfg->right_count - 1) xr += pad;
    }

    /* CENTER: fill remaining space */
    if (cfg->center_count > 0) {
        float center_w = right_start - x_left - pad * 2;
        if (center_w > 0) {
            b->ops->draw_line(b, ctx, x_left, g.y + 6, x_left, g.y + g.h - 6, 1.0f,
                               tc->r, tc->g, tc->b, 0.2f);
            float xc = x_left + pad * 2;
            for (int i = 0; i < cfg->center_count; i++)
                xc = render_widget(&p, cfg->center_widgets[i], xc, center_w);
        }
    }
}

/* ============================================================
 * Panel click handling
 * ============================================================ */

bool vgp_panel_click(const vgp_config_panel_t *cfg,
                      const vgp_theme_t *theme,
                      float local_x, float local_y,
                      uint32_t output_w, uint32_t output_h,
                      struct vgp_server *server,
                      int output_idx)
{
    vgp_panel_geom_t g = vgp_panel_geometry(cfg, theme, output_w, output_h);

    /* Check if click is inside panel rect */
    if (local_y < g.y || local_y >= g.y + g.h)
        return false;

    float pad = 6.0f;
    float ws_btn_w = 22.0f;

    /* Walk through left widgets to find click zones */
    float x = pad;

    /* Check left widgets */
    for (int wi = 0; wi < cfg->left_count; wi++) {
        const char *name = cfg->left_widgets[wi];

        if (strcmp(name, "workspaces") == 0) {
            float ws_area_end = x + 9.0f * (ws_btn_w + 2.0f);
            if (local_x >= x && local_x < ws_area_end) {
                int ws_idx = (int)((local_x - x) / (ws_btn_w + 2.0f));
                if (ws_idx >= 0 && ws_idx < 9) {
                    server->compositor.outputs[output_idx].workspace = ws_idx;
                    VGP_LOG_INFO(TAG, "panel: output %d -> workspace %d", output_idx, ws_idx);
                    vgp_renderer_schedule_frame(&server->renderer);
                    return true;
                }
            }
            x = ws_area_end;
        } else {
            x += 50.0f; /* generic widget width */
        }
        x += pad * 3;
    }

    /* Check right widgets */
    float right_total = 0;
    for (int i = 0; i < cfg->right_count; i++) {
        const char *name = cfg->right_widgets[i];
        if (strcmp(name, "clock") == 0) right_total += 50.0f;
        else if (strcmp(name, "date") == 0) right_total += 50.0f;
        else if (strcmp(name, "cpu") == 0) right_total += 60.0f;
        else if (strcmp(name, "memory") == 0) right_total += 70.0f;
        else if (strcmp(name, "battery") == 0) right_total += 70.0f;
        else if (strcmp(name, "volume") == 0) right_total += 70.0f;
        else if (strcmp(name, "network") == 0) right_total += 80.0f;
        else right_total += 50.0f;
        if (i < cfg->right_count - 1) right_total += pad * 3;
    }
    float right_start = g.w - pad - right_total;

    if (local_x >= right_start) {
        float rx = right_start;
        for (int i = 0; i < cfg->right_count; i++) {
            const char *name = cfg->right_widgets[i];
            float ww = 50.0f;
            if (strcmp(name, "cpu") == 0) ww = 60.0f;
            else if (strcmp(name, "memory") == 0 || strcmp(name, "battery") == 0 || strcmp(name, "volume") == 0) ww = 70.0f;
            else if (strcmp(name, "network") == 0) ww = 80.0f;

            if (local_x >= rx && local_x < rx + ww) {
                if (strcmp(name, "clock") == 0 || strcmp(name, "date") == 0) {
                    /* Toggle calendar */
                    float cal_y = g.top ? g.y + g.h + 4.0f : g.y - 214.0f;
                    vgp_calendar_toggle(&server->calendar, rx, cal_y);
                    vgp_renderer_schedule_frame(&server->renderer);
                    return true;
                }
                /* Other right widgets: no click action */
                return true;
            }
            rx += ww;
            if (i < cfg->right_count - 1) rx += pad * 3;
        }
    }

    /* Center area: taskbar clicks */
    float center_start = x + pad * 2;
    float center_end = right_start - pad;
    float center_w = center_end - center_start;

    if (local_x >= center_start && local_x < center_end && center_w > 0) {
        int ws = server->compositor.outputs[output_idx].workspace;
        int win_count = 0;
        for (int i = 0; i < server->compositor.window_count; i++) {
            vgp_window_t *w = server->compositor.z_order[i];
            if (w->visible && w->workspace == ws && w->decorated)
                win_count++;
        }
        if (win_count > 0) {
            float entry_w = center_w / (float)win_count;
            if (entry_w > 250.0f) entry_w = 250.0f;
            int clicked_idx = (int)((local_x - center_start) / entry_w);
            int found = 0;
            for (int i = 0; i < server->compositor.window_count; i++) {
                vgp_window_t *w = server->compositor.z_order[i];
                if (!w->visible || w->workspace != ws || !w->decorated) continue;
                if (found == clicked_idx) {
                    vgp_compositor_focus_window(&server->compositor, w);
                    vgp_renderer_schedule_frame(&server->renderer);
                    return true;
                }
                found++;
            }
        }
    }

    /* Click was in panel but not on any interactive widget */
    return true;
}
