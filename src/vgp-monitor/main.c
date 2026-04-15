/* VGP System Monitor -- CPU, RAM, process list, live graphs */

#include "vgp-ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HISTORY_LEN 60

typedef struct {
    float cpu_history[HISTORY_LEN];
    float mem_history[HISTORY_LEN];
    int   history_idx;

    float cpu_percent;
    float mem_percent;
    long  mem_total_mb;
    long  mem_used_mb;

    /* Previous CPU values for delta calculation */
    long  prev_idle, prev_total;

    /* Process list */
    struct { int pid; char name[64]; float cpu; float mem; } procs[50];
    int proc_count;

    int scroll_offset;
} monitor_state_t;

static monitor_state_t mon;

static void read_cpu(void)
{
    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;
    char line[256];
    if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
    fclose(f);

    long user, nice, system, idle, iowait, irq, softirq;
    sscanf(line, "cpu %ld %ld %ld %ld %ld %ld %ld",
           &user, &nice, &system, &idle, &iowait, &irq, &softirq);

    long total = user + nice + system + idle + iowait + irq + softirq;
    long diff_idle = idle - mon.prev_idle;
    long diff_total = total - mon.prev_total;

    if (diff_total > 0)
        mon.cpu_percent = 100.0f * (1.0f - (float)diff_idle / (float)diff_total);

    mon.prev_idle = idle;
    mon.prev_total = total;
}

static void read_mem(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;
    char line[256];
    long total = 0, available = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line + 9, " %ld", &total);
        else if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line + 13, " %ld", &available);
    }
    fclose(f);

    mon.mem_total_mb = total / 1024;
    mon.mem_used_mb = (total - available) / 1024;
    if (total > 0)
        mon.mem_percent = 100.0f * (float)(total - available) / (float)total;
}

static void update_history(void)
{
    mon.cpu_history[mon.history_idx] = mon.cpu_percent;
    mon.mem_history[mon.history_idx] = mon.mem_percent;
    mon.history_idx = (mon.history_idx + 1) % HISTORY_LEN;
}

static void draw_graph(vui_ctx_t *ctx, int row, int col, int width, int height,
                        float *data, int data_len, int data_idx,
                        vui_color_t fg, const char *label)
{
    /* Label */
    vui_text_bold(ctx, row, col, label, fg, VUI_BG);

    /* Graph area */
    int graph_row = row + 1;
    vui_box(ctx, graph_row, col, height, width, VUI_BORDER, VUI_BG);

    for (int x = 1; x < width - 1; x++) {
        int di = (data_idx - (width - 1 - x) + data_len) % data_len;
        float val = data[di] / 100.0f;
        int bar_h = (int)(val * (float)(height - 2));

        for (int y = 0; y < height - 2; y++) {
            int r = graph_row + height - 2 - y;
            if (y < bar_h)
                vui_set_cell(ctx, r, col + x, 0x2588, fg, VUI_BG, 0); /* █ */
        }
    }
}

static void render(vui_ctx_t *ctx)
{
    /* Update system stats */
    read_cpu();
    read_mem();
    update_history();

    vui_clear(ctx, VUI_BG);

    /* Title */
    vui_fill(ctx, 0, 0, 1, ctx->cols, VUI_SURFACE);
    vui_text_bold(ctx, 0, 2, " VGP System Monitor ", VUI_ACCENT, VUI_SURFACE);

    int w = ctx->cols;
    int graph_w = w / 2 - 3;
    int graph_h = 12;

    /* CPU graph */
    draw_graph(ctx, 2, 2, graph_w, graph_h,
               mon.cpu_history, HISTORY_LEN, mon.history_idx,
               VUI_ACCENT, "CPU");

    char cpu_str[32];
    snprintf(cpu_str, sizeof(cpu_str), " %.1f%%", mon.cpu_percent);
    vui_text(ctx, 2, 6, cpu_str, VUI_WHITE, VUI_BG);

    /* Memory graph */
    draw_graph(ctx, 2, w / 2 + 1, graph_w, graph_h,
               mon.mem_history, HISTORY_LEN, mon.history_idx,
               VUI_GREEN, "Memory");

    char mem_str[64];
    snprintf(mem_str, sizeof(mem_str), " %ldMB / %ldMB (%.0f%%)",
             mon.mem_used_mb, mon.mem_total_mb, mon.mem_percent);
    vui_text(ctx, 2, w / 2 + 9, mem_str, VUI_WHITE, VUI_BG);

    /* System info below graphs */
    int info_row = 2 + graph_h + 2;
    vui_section(ctx, info_row, 2, w - 4, "System", VUI_GRAY);
    info_row += 2;

    char hostname[64] = "";
    gethostname(hostname, sizeof(hostname));
    char info_buf[128];
    snprintf(info_buf, sizeof(info_buf), "Host: %s", hostname);
    vui_label(ctx, info_row++, 4, info_buf, VUI_GRAY);

    /* CPU and memory bars */
    info_row++;
    vui_label(ctx, info_row, 4, "CPU:", VUI_GRAY);
    vui_progress(ctx, info_row++, 10, w - 14, mon.cpu_percent / 100.0f, VUI_ACCENT, VUI_BORDER);

    vui_label(ctx, info_row, 4, "RAM:", VUI_GRAY);
    vui_progress(ctx, info_row++, 10, w - 14, mon.mem_percent / 100.0f, VUI_GREEN, VUI_BORDER);

    /* Bottom status */
    vui_fill(ctx, ctx->rows - 1, 0, 1, ctx->cols, VUI_SURFACE);
    vui_text(ctx, ctx->rows - 1, 2, "Press Escape to close | Auto-refreshing",
              VUI_GRAY, VUI_SURFACE);

    if (ctx->key_pressed && ctx->last_keysym == 0xFF1B)
        ctx->running = false;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    FILE *logfile = fopen("/tmp/vgp-monitor.log", "w");
    if (logfile) { setvbuf(logfile, NULL, _IOLBF, 0); dup2(fileno(logfile), STDERR_FILENO); fclose(logfile); }

    memset(&mon, 0, sizeof(mon));
    /* Initial CPU read (need two reads for delta) */
    read_cpu();

    vui_ctx_t ctx;
    if (vui_init(&ctx, "System Monitor", 800, 500) < 0)
        return 1;

    /* Run at ~2fps for stats update (not 60fps, saves CPU) */
    while (ctx.running) {
        vui_poll(&ctx, 500); /* 500ms = 2 updates per second */
        vui_begin_frame(&ctx);
        render(&ctx);
        vui_end_frame(&ctx);
    }

    vui_destroy(&ctx);
    return 0;
}
