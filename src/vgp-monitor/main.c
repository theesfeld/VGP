/* VGP System Monitor -- Fighter Jet HUD Style
 * Animated vector display: moving tape scales, compass arcs,
 * tick marks, angular brackets. NERV/F-16 aesthetic.
 * Everything is lines and text -- pure vector strokes. */

#include "vgp-gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define HISTORY 240

static struct {
    float cpu[HISTORY];
    float mem[HISTORY];
    float net_rx[HISTORY]; /* bytes/sec normalized 0-1 */
    float net_tx[HISTORY];
    int   head;
    long  prev_idle, prev_total;
    long  prev_rx, prev_tx;
    float uptime;
    int   num_procs;
    float load_avg;
    float frame_time;
} mon;

static void sample(void)
{
    /* CPU */
    FILE *f = fopen("/proc/stat", "r");
    if (f) {
        long user, nice, sys, idle, iow, irq, sirq, steal;
        if (fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
                   &user, &nice, &sys, &idle, &iow, &irq, &sirq, &steal) == 8) {
            long total = user + nice + sys + idle + iow + irq + sirq + steal;
            long di = idle - mon.prev_idle, dt = total - mon.prev_total;
            if (dt > 0) mon.cpu[mon.head] = (float)(dt - di) / (float)dt;
            mon.prev_idle = idle; mon.prev_total = total;
        }
        fclose(f);
    }

    /* Memory */
    long mtotal = 0, mavail = 0;
    f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &mtotal);
            else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &mavail);
        }
        fclose(f);
    }
    if (mtotal > 0) mon.mem[mon.head] = (float)(mtotal - mavail) / (float)mtotal;

    /* Network (rx/tx bytes from first non-lo interface) */
    f = fopen("/proc/net/dev", "r");
    if (f) {
        char line[512];
        fgets(line, sizeof(line), f); fgets(line, sizeof(line), f); /* skip headers */
        while (fgets(line, sizeof(line), f)) {
            char iface[32]; long rx = 0, tx = 0;
            if (sscanf(line, " %31[^:]:%ld %*d %*d %*d %*d %*d %*d %*d %ld",
                        iface, &rx, &tx) >= 3) {
                /* Skip loopback */
                size_t len = strlen(iface);
                while (len > 0 && iface[len-1] == ' ') iface[--len] = '\0';
                if (strcmp(iface, "lo") == 0) continue;
                long drx = rx - mon.prev_rx;
                long dtx = tx - mon.prev_tx;
                if (mon.prev_rx > 0 && drx >= 0) {
                    mon.net_rx[mon.head] = (float)drx / 1000000.0f; /* normalize to MB/s-ish */
                    if (mon.net_rx[mon.head] > 1.0f) mon.net_rx[mon.head] = 1.0f;
                }
                if (mon.prev_tx > 0 && dtx >= 0) {
                    mon.net_tx[mon.head] = (float)dtx / 1000000.0f;
                    if (mon.net_tx[mon.head] > 1.0f) mon.net_tx[mon.head] = 1.0f;
                }
                mon.prev_rx = rx; mon.prev_tx = tx;
                break;
            }
        }
        fclose(f);
    }

    /* Uptime + load */
    f = fopen("/proc/uptime", "r");
    if (f) { fscanf(f, "%f", &mon.uptime); fclose(f); }
    f = fopen("/proc/loadavg", "r");
    if (f) { fscanf(f, "%f", &mon.load_avg); fclose(f); }

    /* Process count */
    f = popen("ls -1d /proc/[0-9]* 2>/dev/null | wc -l", "r");
    if (f) { fscanf(f, "%d", &mon.num_procs); pclose(f); }

    mon.head = (mon.head + 1) % HISTORY;
}

/* ============================================================
 * HUD Drawing Primitives
 * ============================================================ */

static vgfx_color_t hud_green;
static vgfx_color_t hud_dim;
static vgfx_color_t hud_bright;
static vgfx_color_t hud_warn;
static vgfx_color_t hud_bg;

/* Vertical tape scale: moving ladder showing a value 0-100 */
static void draw_tape_v(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                          float value, const char *label, bool right_side)
{
    float lw = 1.2f;
    /* Border */
    vgfx_rect_outline(ctx, x, y, w, h, lw, hud_green);

    /* Label at top */
    float fs = ctx->theme.font_size - 2;
    vgfx_text_bold(ctx, label, x + 4, y - 2, fs, hud_green);

    /* Current value box */
    float box_h = 20;
    float box_y = y + h * 0.5f - box_h * 0.5f;
    char val[16]; snprintf(val, sizeof(val), "%3.0f", value);
    if (right_side) {
        vgfx_rect(ctx, x + w, box_y, 40, box_h, hud_bg);
        vgfx_rect_outline(ctx, x + w, box_y, 40, box_h, lw, hud_bright);
        vgfx_text_bold(ctx, val, x + w + 4, box_y + 15, fs, hud_bright);
        /* Arrow */
        vgfx_line(ctx, x + w, box_y + box_h * 0.5f, x + w - 6, box_y + box_h * 0.5f - 4, lw, hud_bright);
        vgfx_line(ctx, x + w, box_y + box_h * 0.5f, x + w - 6, box_y + box_h * 0.5f + 4, lw, hud_bright);
    } else {
        vgfx_rect(ctx, x - 40, box_y, 40, box_h, hud_bg);
        vgfx_rect_outline(ctx, x - 40, box_y, 40, box_h, lw, hud_bright);
        vgfx_text_bold(ctx, val, x - 36, box_y + 15, fs, hud_bright);
        vgfx_line(ctx, x, box_y + box_h * 0.5f, x + 6, box_y + box_h * 0.5f - 4, lw, hud_bright);
        vgfx_line(ctx, x, box_y + box_h * 0.5f, x + 6, box_y + box_h * 0.5f + 4, lw, hud_bright);
    }

    /* Scrolling tick marks */
    vgfx_push_clip(ctx, x + 1, y + 1, w - 2, h - 2);
    float center_y = y + h * 0.5f;
    float px_per_unit = h / 60.0f; /* show ~60 units in view */
    float offset = value * px_per_unit;
    int start = (int)(value - 30);
    int end = (int)(value + 30);

    for (int v = start; v <= end; v++) {
        if (v < 0 || v > 100) continue;
        float ty = center_y - ((float)v - value) * px_per_unit;
        bool major = (v % 10 == 0);
        bool minor = (v % 5 == 0);
        float tick_w = major ? w * 0.4f : (minor ? w * 0.25f : w * 0.12f);
        vgfx_color_t tc = (value > 80 && v >= 80) ? hud_warn : hud_green;

        if (right_side) {
            vgfx_line(ctx, x, ty, x + tick_w, ty, major ? 1.5f : 0.8f, tc);
        } else {
            vgfx_line(ctx, x + w - tick_w, ty, x + w, ty, major ? 1.5f : 0.8f, tc);
        }

        if (major) {
            char num[8]; snprintf(num, sizeof(num), "%d", v);
            float tx = right_side ? x + tick_w + 3 : x + w - tick_w - 24;
            vgfx_text(ctx, num, tx, ty + 4, fs - 2, tc);
        }
    }
    vgfx_pop_clip(ctx);
}

/* Horizontal trace: scrolling waveform */
static void draw_trace(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                         float *data, int head, vgfx_color_t color, const char *label)
{
    vgfx_rect_outline(ctx, x, y, w, h, 0.8f, hud_dim);
    float fs = ctx->theme.font_size - 3;
    vgfx_text(ctx, label, x + 4, y + fs + 2, fs, color);

    /* Grid lines */
    for (int i = 1; i < 4; i++) {
        float gy = y + h * (float)i / 4.0f;
        vgfx_line(ctx, x, gy, x + w, gy, 0.3f, hud_dim);
    }

    /* Trace line */
    float step = w / (float)(HISTORY - 1);
    for (int i = 1; i < HISTORY; i++) {
        int i0 = (head + i - 1) % HISTORY, i1 = (head + i) % HISTORY;
        float x0 = x + (float)(i-1) * step, x1 = x + (float)i * step;
        float y0 = y + h * (1.0f - data[i0]), y1 = y + h * (1.0f - data[i1]);
        vgfx_line(ctx, x0, y0, x1, y1, 1.2f, color);
    }
}

/* Compass-style arc: shows a value 0-360 with rotating marks */
static void draw_compass(vgfx_ctx_t *ctx, float cx, float cy, float r,
                            float value, const char *label)
{
    float fs = ctx->theme.font_size - 2;

    /* Arc (approximate with line segments) */
    int segs = 48;
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i / (float)segs * 2.0f * (float)M_PI;
        float a1 = (float)(i+1) / (float)segs * 2.0f * (float)M_PI;
        vgfx_line(ctx, cx + r * cosf(a0), cy + r * sinf(a0),
                    cx + r * cosf(a1), cy + r * sinf(a1), 1.0f, hud_green);
    }

    /* Tick marks around the circle */
    for (int deg = 0; deg < 360; deg += 10) {
        float a = (float)deg * (float)M_PI / 180.0f - (float)M_PI * 0.5f;
        bool major = (deg % 30 == 0);
        float inner = major ? r - 10 : r - 5;
        vgfx_line(ctx, cx + inner * cosf(a), cy + inner * sinf(a),
                    cx + r * cosf(a), cy + r * sinf(a),
                    major ? 1.2f : 0.6f, hud_green);
    }

    /* Pointer needle */
    float needle_a = value * 2.0f * (float)M_PI / 100.0f - (float)M_PI * 0.5f;
    vgfx_line(ctx, cx, cy, cx + (r - 15) * cosf(needle_a), cy + (r - 15) * sinf(needle_a),
                2.0f, hud_bright);

    /* Center dot */
    vgfx_circle(ctx, cx, cy, 3, hud_bright);

    /* Label and value */
    char val[16]; snprintf(val, sizeof(val), "%.0f%%", value);
    float tw = vgfx_text_width(ctx, val, -1, fs + 2);
    vgfx_text_bold(ctx, val, cx - tw * 0.5f, cy + r + 18, fs + 2, hud_bright);
    tw = vgfx_text_width(ctx, label, -1, fs);
    vgfx_text(ctx, label, cx - tw * 0.5f, cy + r + 32, fs, hud_green);
}

/* ============================================================
 * Main HUD Render
 * ============================================================ */

static void render(vgfx_ctx_t *ctx)
{
    static int tick = 0;
    static float anim_t = 0;
    anim_t += 0.016f;
    if (++tick >= 60) { sample(); tick = 0; }

    /* HUD colors */
    hud_green  = vgfx_rgba(0.0f, 0.85f, 0.0f, 0.9f);
    hud_dim    = vgfx_rgba(0.0f, 0.4f, 0.0f, 0.5f);
    hud_bright = vgfx_rgba(0.0f, 1.0f, 0.0f, 1.0f);
    hud_warn   = vgfx_rgba(1.0f, 0.8f, 0.0f, 1.0f);
    hud_bg     = vgfx_rgba(0.0f, 0.02f, 0.0f, 1.0f);

    float w = ctx->width, h = ctx->height;
    float fs = ctx->theme.font_size;

    /* Black background */
    vgfx_clear(ctx, hud_bg);

    /* Scanline effect: subtle horizontal lines */
    for (float sy = 0; sy < h; sy += 3.0f) {
        vgfx_line(ctx, 0, sy, w, sy, 0.3f, vgfx_rgba(0, 0.05f, 0, 0.3f));
    }

    float cpu_val = mon.cpu[(mon.head + HISTORY - 1) % HISTORY] * 100.0f;
    float mem_val = mon.mem[(mon.head + HISTORY - 1) % HISTORY] * 100.0f;

    /* === LEFT TAPE: CPU === */
    draw_tape_v(ctx, 8, 40, 30, h - 100, cpu_val, "CPU", false);

    /* === RIGHT TAPE: MEM === */
    draw_tape_v(ctx, w - 38, 40, 30, h - 100, mem_val, "MEM", true);

    /* === CENTER: Compass gauge for load average === */
    float cx = w * 0.5f, cy = h * 0.38f;
    float compass_r = (w < h ? w : h) * 0.18f;
    draw_compass(ctx, cx, cy, compass_r, mon.load_avg * 25.0f, "LOAD");

    /* === BOTTOM: Trace waveforms === */
    float trace_y = h * 0.65f;
    float trace_h = h * 0.14f;
    float trace_w = w * 0.45f;

    draw_trace(ctx, 55, trace_y, trace_w, trace_h,
                mon.cpu, mon.head, hud_green, "CPU TRACE");
    draw_trace(ctx, w - trace_w - 55, trace_y, trace_w, trace_h,
                mon.mem, mon.head, hud_bright, "MEM TRACE");

    /* Network traces below */
    float net_y = trace_y + trace_h + 10;
    float net_h = h * 0.10f;
    draw_trace(ctx, 55, net_y, trace_w, net_h,
                mon.net_rx, mon.head, vgfx_rgba(0.0f, 0.7f, 1.0f, 0.8f), "NET RX");
    draw_trace(ctx, w - trace_w - 55, net_y, trace_w, net_h,
                mon.net_tx, mon.head, vgfx_rgba(1.0f, 0.5f, 0.0f, 0.8f), "NET TX");

    /* === TOP CENTER: System info === */
    {
        int up_h = (int)(mon.uptime / 3600);
        int up_m = (int)(mon.uptime / 60) % 60;
        char info[128];
        snprintf(info, sizeof(info), "UP %02d:%02d  PROCS %d  LOAD %.2f",
                 up_h, up_m, mon.num_procs, mon.load_avg);
        float tw = vgfx_text_width(ctx, info, -1, fs - 2);
        vgfx_text(ctx, info, w * 0.5f - tw * 0.5f, 20, fs - 2, hud_green);
    }

    /* === Crosshair at center === */
    {
        float ch_size = 8;
        /* Small cross */
        vgfx_line(ctx, cx - ch_size, cy, cx - 3, cy, 1, hud_dim);
        vgfx_line(ctx, cx + 3, cy, cx + ch_size, cy, 1, hud_dim);
        vgfx_line(ctx, cx, cy - ch_size, cx, cy - 3, 1, hud_dim);
        vgfx_line(ctx, cx, cy + 3, cx, cy + ch_size, 1, hud_dim);
    }

    /* === Corner brackets (HUD frame) === */
    {
        float bsz = 20, blw = 1.5f;
        /* Top-left */
        vgfx_line(ctx, 2, 2, 2 + bsz, 2, blw, hud_green);
        vgfx_line(ctx, 2, 2, 2, 2 + bsz, blw, hud_green);
        /* Top-right */
        vgfx_line(ctx, w - 2, 2, w - 2 - bsz, 2, blw, hud_green);
        vgfx_line(ctx, w - 2, 2, w - 2, 2 + bsz, blw, hud_green);
        /* Bottom-left */
        vgfx_line(ctx, 2, h - 2, 2 + bsz, h - 2, blw, hud_green);
        vgfx_line(ctx, 2, h - 2, 2, h - 2 - bsz, blw, hud_green);
        /* Bottom-right */
        vgfx_line(ctx, w - 2, h - 2, w - 2 - bsz, h - 2, blw, hud_green);
        vgfx_line(ctx, w - 2, h - 2, w - 2, h - 2 - bsz, blw, hud_green);
    }

    /* === Bottom status line === */
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char status[64];
        snprintf(status, sizeof(status), "VGP SYS MON  %02d:%02d:%02d",
                 t->tm_hour, t->tm_min, t->tm_sec);
        vgfx_text(ctx, status, 55, h - 12, fs - 3, hud_dim);

        /* Blinking indicator */
        if ((int)(anim_t * 2) % 2 == 0)
            vgfx_circle(ctx, w - 20, h - 12, 3, hud_green);
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sample(); /* initial sample */

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Monitor", 800, 600, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
