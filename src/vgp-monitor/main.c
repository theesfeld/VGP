/* VGP System Monitor -- GPU-rendered graphical system monitor */

#include "vgp-gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HISTORY 120

static struct {
    float cpu_history[HISTORY];
    float mem_history[HISTORY];
    int   head;
    long  prev_idle, prev_total;
} mon;

static void sample(void)
{
    FILE *f = fopen("/proc/stat", "r");
    if (f) {
        long user, nice, sys, idle, iow, irq, sirq, steal;
        if (fscanf(f, "cpu %ld %ld %ld %ld %ld %ld %ld %ld",
                   &user, &nice, &sys, &idle, &iow, &irq, &sirq, &steal) == 8) {
            long total = user + nice + sys + idle + iow + irq + sirq + steal;
            long di = idle - mon.prev_idle, dt = total - mon.prev_total;
            if (dt > 0) mon.cpu_history[mon.head] = (float)(dt - di) / (float)dt;
            mon.prev_idle = idle; mon.prev_total = total;
        }
        fclose(f);
    }
    long total = 0, avail = 0;
    f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, "%ld", &total);
            else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, "%ld", &avail);
        }
        fclose(f);
    }
    if (total > 0) mon.mem_history[mon.head] = (float)(total - avail) / (float)total;
    mon.head = (mon.head + 1) % HISTORY;
}

static void draw_graph(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                         const char *title, float *data, int head, vgfx_color_t color)
{
    vgfx_rounded_rect(ctx, x, y, w, h, 6, vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));
    vgfx_rounded_rect_outline(ctx, x, y, w, h, 6, 1, vgfx_theme_color(ctx, VGP_THEME_BORDER));

    vgfx_text_bold(ctx, title, x + 10, y + 18, ctx->theme.font_size,
                     vgfx_theme_color(ctx, VGP_THEME_FG));

    float current = data[(head + HISTORY - 1) % HISTORY];
    char val[16]; snprintf(val, sizeof(val), "%.0f%%", current * 100);
    float vw = vgfx_text_width(ctx, val, -1, ctx->theme.font_size_large);
    vgfx_text_bold(ctx, val, x + w - vw - 10, y + 20, ctx->theme.font_size_large, color);

    float gx = x + 10, gy = y + 30, gw = w - 20, gh = h - 40;

    for (int i = 1; i < 4; i++) {
        float ly = gy + gh * (float)i / 4.0f;
        vgfx_line(ctx, gx, ly, gx + gw, ly, 0.5f,
                    vgfx_alpha(vgfx_theme_color(ctx, VGP_THEME_BORDER), 0.3f));
    }

    float step = gw / (float)(HISTORY - 1);
    for (int i = 0; i < HISTORY; i++) {
        int idx = (head + i) % HISTORY;
        float fx = gx + (float)i * step;
        float fy = gy + gh * (1.0f - data[idx]);
        float fh = gy + gh - fy;
        if (fh > 0) vgfx_rect(ctx, fx, fy, step + 1, fh, vgfx_alpha(color, 0.15f));
    }

    for (int i = 1; i < HISTORY; i++) {
        int i0 = (head + i - 1) % HISTORY, i1 = (head + i) % HISTORY;
        vgfx_line(ctx, gx + (float)(i-1)*step, gy + gh*(1-data[i0]),
                    gx + (float)i*step, gy + gh*(1-data[i1]), 2.0f, color);
    }
}

static void render(vgfx_ctx_t *ctx)
{
    static int tick = 0;
    if (++tick >= 60) { sample(); tick = 0; }

    vgfx_clear(ctx, vgfx_theme_color(ctx, VGP_THEME_BG));
    float p = ctx->theme.padding;

    vgfx_text_bold(ctx, "System Monitor", p, p + ctx->theme.font_size_large,
                     ctx->theme.font_size_large, vgfx_theme_color(ctx, VGP_THEME_ACCENT));

    float gy = p * 2 + ctx->theme.font_size_large + 10;
    float gh = (ctx->height - gy - p * 2) * 0.5f - p;
    float gw = ctx->width - p * 2;

    draw_graph(ctx, p, gy, gw, gh, "CPU", mon.cpu_history, mon.head,
                vgfx_theme_color(ctx, VGP_THEME_ACCENT));
    draw_graph(ctx, p, gy + gh + p * 2, gw, gh, "Memory", mon.mem_history, mon.head,
                vgfx_theme_color(ctx, VGP_THEME_SUCCESS));
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sample();
    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Monitor", 700, 500, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
