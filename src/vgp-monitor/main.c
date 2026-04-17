/* VGP System Monitor -- F-16 Engine Page Style
 *
 * Clean labeled bar gauges. Every element signifies data.
 *
 * Layout:
 *   Top row: CPU | MEM | GPU | SWP -- labeled vertical bar gauges
 *   Middle:  Per-core bars (0, 1, 2, ..., N) with numeric labels
 *   Bottom:  Data fields -- hostname, uptime, load, processes, net, disk
 */

#include "vgp-gfx.h"
#include "vgp-hud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_CORES 64

/* ============================================================
 * Data collection
 * ============================================================ */

static struct {
    /* Aggregate CPU */
    float cpu_total;
    long  prev_idle, prev_total;

    /* Per-core */
    float core[MAX_CORES];
    long  core_prev_idle[MAX_CORES];
    long  core_prev_total[MAX_CORES];
    int   num_cores;

    /* Memory */
    long mem_total_kb, mem_avail_kb;
    long swap_total_kb, swap_free_kb;
    float mem_pct, swap_pct;

    /* GPU */
    float gpu_pct;
    bool  gpu_ok;

    /* Network */
    long prev_rx, prev_tx;
    long rx_bytes_per_sec, tx_bytes_per_sec;

    /* Disk */
    long prev_reads, prev_writes;
    long disk_reads_per_sec, disk_writes_per_sec;

    /* System */
    float uptime;
    float load[3];
    int   procs;
    char  hostname[64];
    char  kernel[64];
} D;

static void sample(void)
{
    /* CPU aggregate + per-core */
    FILE *f = fopen("/proc/stat", "r");
    if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu", 3) != 0) break;
            long u,n,s,id,io,ir,si,st;
            if (line[3] == ' ') {
                if (sscanf(line+4, "%ld %ld %ld %ld %ld %ld %ld %ld", &u,&n,&s,&id,&io,&ir,&si,&st) == 8) {
                    long total = u+n+s+id+io+ir+si+st;
                    long di = id - D.prev_idle, dt = total - D.prev_total;
                    if (dt > 0) D.cpu_total = (float)(dt-di)/(float)dt;
                    D.prev_idle = id; D.prev_total = total;
                }
            } else {
                int ci = 0;
                if (sscanf(line, "cpu%d %ld %ld %ld %ld %ld %ld %ld %ld", &ci, &u,&n,&s,&id,&io,&ir,&si,&st) == 9) {
                    if (ci < MAX_CORES) {
                        long total = u+n+s+id+io+ir+si+st;
                        long di = id - D.core_prev_idle[ci];
                        long dt = total - D.core_prev_total[ci];
                        if (dt > 0) D.core[ci] = (float)(dt-di)/(float)dt;
                        D.core_prev_idle[ci] = id;
                        D.core_prev_total[ci] = total;
                        if (ci + 1 > D.num_cores) D.num_cores = ci + 1;
                    }
                }
            }
        }
        fclose(f);
    }

    /* Memory */
    f = fopen("/proc/meminfo", "r");
    if (f) {
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line,"MemTotal:",9)==0) sscanf(line+9,"%ld",&D.mem_total_kb);
            else if (strncmp(line,"MemAvailable:",13)==0) sscanf(line+13,"%ld",&D.mem_avail_kb);
            else if (strncmp(line,"SwapTotal:",10)==0) sscanf(line+10,"%ld",&D.swap_total_kb);
            else if (strncmp(line,"SwapFree:",9)==0) sscanf(line+9,"%ld",&D.swap_free_kb);
        }
        fclose(f);
        if (D.mem_total_kb > 0) D.mem_pct = (float)(D.mem_total_kb - D.mem_avail_kb) / (float)D.mem_total_kb;
        if (D.swap_total_kb > 0) D.swap_pct = (float)(D.swap_total_kb - D.swap_free_kb) / (float)D.swap_total_kb;
    }

    /* GPU */
    D.gpu_ok = false;
    f = fopen("/sys/class/drm/card0/device/gpu_busy_percent", "r");
    if (!f) f = fopen("/sys/class/drm/card1/device/gpu_busy_percent", "r");
    if (f) { int g; if (fscanf(f,"%d",&g)==1) { D.gpu_pct = (float)g/100.0f; D.gpu_ok = true; } fclose(f); }

    /* Network */
    f = fopen("/proc/net/dev", "r");
    if (f) {
        char line[512];
        fgets(line,sizeof(line),f); fgets(line,sizeof(line),f);
        long rx_total = 0, tx_total = 0;
        while (fgets(line,sizeof(line),f)) {
            char iface[32]; long rx=0, tx=0;
            if (sscanf(line," %31[^:]:%ld %*d %*d %*d %*d %*d %*d %*d %ld", iface,&rx,&tx)>=3) {
                char *p = iface; while(*p==' ') p++;
                if (strcmp(p,"lo")==0) continue;
                rx_total += rx; tx_total += tx;
            }
        }
        fclose(f);
        if (D.prev_rx > 0) {
            D.rx_bytes_per_sec = rx_total - D.prev_rx;
            D.tx_bytes_per_sec = tx_total - D.prev_tx;
            if (D.rx_bytes_per_sec < 0) D.rx_bytes_per_sec = 0;
            if (D.tx_bytes_per_sec < 0) D.tx_bytes_per_sec = 0;
        }
        D.prev_rx = rx_total; D.prev_tx = tx_total;
    }

    /* Disk I/O (first device from /proc/diskstats) */
    f = fopen("/proc/diskstats", "r");
    if (f) {
        char line[256];
        long reads_total = 0, writes_total = 0;
        while (fgets(line, sizeof(line), f)) {
            int maj, min;
            char dev[32];
            long reads, writes;
            if (sscanf(line, "%d %d %31s %ld %*d %*d %*d %ld", &maj, &min, dev, &reads, &writes) == 5) {
                /* Only physical disks (skip loop, ram, dm-*) */
                if (dev[0] == 'l' || dev[0] == 'r' || (dev[0] == 'd' && dev[1] == 'm')) continue;
                /* Skip partitions (sda1, nvme0n1p1) -- only whole disks */
                size_t len = strlen(dev);
                if (len > 0 && dev[len-1] >= '0' && dev[len-1] <= '9' && dev[0] != 'n') continue;
                reads_total += reads;
                writes_total += writes;
            }
        }
        fclose(f);
        if (D.prev_reads > 0) {
            D.disk_reads_per_sec = reads_total - D.prev_reads;
            D.disk_writes_per_sec = writes_total - D.prev_writes;
            if (D.disk_reads_per_sec < 0) D.disk_reads_per_sec = 0;
            if (D.disk_writes_per_sec < 0) D.disk_writes_per_sec = 0;
        }
        D.prev_reads = reads_total; D.prev_writes = writes_total;
    }

    /* Uptime / load / procs */
    f = fopen("/proc/uptime", "r");
    if (f) { fscanf(f,"%f",&D.uptime); fclose(f); }
    f = fopen("/proc/loadavg", "r");
    if (f) { fscanf(f,"%f %f %f",&D.load[0],&D.load[1],&D.load[2]); fclose(f); }
    if (!D.hostname[0]) gethostname(D.hostname, sizeof(D.hostname));
    if (!D.kernel[0]) {
        f = fopen("/proc/sys/kernel/osrelease", "r");
        if (f) { fscanf(f, "%63s", D.kernel); fclose(f); }
    }
    f = popen("ls -1d /proc/[0-9]* 2>/dev/null | wc -l", "r");
    if (f) { fscanf(f,"%d",&D.procs); pclose(f); }
}

/* ============================================================
 * HUD colors
 * ============================================================ */

/* Palette rule:
 *   DYNAMIC (values, pointers, fills) = white / yellow / red phosphor
 *   STATIC  (labels, frames, tick marks) = black etching
 */
static vgfx_color_t C_FG, C_DIM, C_HI, C_WARN, C_CRIT, C_BG;

static void init_colors(void) {
    C_FG   = vgfx_rgba(0.95f, 0.95f, 0.95f, 1.0f);  /* white phosphor */
    C_DIM  = vgfx_rgba(0.0f,  0.0f,  0.0f,  0.85f); /* static = black etch */
    C_HI   = vgfx_rgba(1.0f,  1.0f,  1.0f,  1.0f);  /* bright white */
    C_WARN = vgfx_rgba(1.0f,  0.85f, 0.0f,  1.0f);  /* yellow phosphor */
    C_CRIT = vgfx_rgba(1.0f,  0.3f,  0.3f,  1.0f);  /* red phosphor */
    C_BG   = vgfx_rgba(0.0f,  0.0f,  0.0f,  0.0f);  /* transparent */
}

static vgfx_color_t usage_color(float v) {
    if (v >= 0.90f) return C_CRIT;
    if (v >= 0.75f) return C_WARN;
    return C_FG;
}

/* ============================================================
 * HUD instrument: F-16 tape scale gauge
 *
 *    LBL     <-  ETCHED label above
 *   +---+
 *   |100|
 *   |---|
 *   | 75|
 *   |   |---[ 67]    <- left-pointing caret with bracketed digital
 *   | 50|   (current value)
 *   |---|
 *   | 25|
 *   |---|
 *   |  0|
 *   +---+
 *
 * The "bar" is a numbered tape scale; the current value is shown as a
 * ">" pointer sliding along the scale with a [XX] readout beside it.
 * ============================================================ */

static void draw_bar_gauge(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                             const char *label, float value /* 0..1 */)
{
    float fs = 13.0f;
    float tick_fs = 10.0f;
    vgfx_color_t col = usage_color(value);
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    /* ETCHED label above the gauge */
    float lw = vgfx_text_width(ctx, label, -1, fs);
    float lx = x + (w - lw) * 0.5f;
    vgfx_text(ctx, label, lx + 0.6f, y + fs + 0.6f, fs, vgfx_rgba(0, 0, 0, 0.5f));
    vgfx_text_bold(ctx, label, lx, y + fs, fs, C_DIM);

    /* Tape area geometry */
    float tape_w = 42.0f;
    float tape_x = x + 4.0f;
    float tape_top = y + fs * 1.8f + 4.0f;
    float tape_bot = y + h - 4.0f;
    float tape_h = tape_bot - tape_top;

    /* Boxed tape outline */
    vgfx_rect_outline(ctx, tape_x, tape_top, tape_w, tape_h, 1.0f, C_DIM);

    /* Tick marks every 10%, labeled every 25% */
    for (int i = 0; i <= 10; i++) {
        float fv = (float)i * 0.1f;
        float ty = tape_bot - fv * tape_h;
        bool major = (i % 5 == 0);
        /* Right-side ticks */
        vgfx_line(ctx, tape_x + tape_w - (major ? 8.0f : 4.0f), ty,
                         tape_x + tape_w, ty,
                         major ? 1.0f : 0.6f, C_DIM);
        /* Left-side ticks */
        vgfx_line(ctx, tape_x, ty,
                         tape_x + (major ? 8.0f : 4.0f), ty,
                         major ? 1.0f : 0.6f, C_DIM);
        if (major) {
            char lbl[6]; snprintf(lbl, sizeof(lbl), "%d", i * 10);
            float tw = vgfx_text_width(ctx, lbl, -1, tick_fs);
            vgfx_text(ctx, lbl, tape_x + (tape_w - tw) * 0.5f,
                       ty + tick_fs * 0.35f, tick_fs, C_DIM);
        }
    }

    /* Current-value pointer: right-pointing caret ">" just outside
     * the tape. Phosphor glow on every dynamic stroke. */
    float py = tape_bot - value * tape_h;
    float px = tape_x + tape_w + 2.0f;
    hud_phosphor_line(ctx, px, py - 5.0f, px + 7.0f, py, 1.4f, col);
    hud_phosphor_line(ctx, px + 7.0f, py, px, py + 5.0f, 1.4f, col);
    /* Horizontal mark line across the tape at the current value */
    hud_phosphor_line(ctx, tape_x + 1.0f, py,
                         tape_x + tape_w - 1.0f, py, 1.0f, col);

    /* Bracketed PROJECTED digital readout -- phosphor glow */
    char val[12]; snprintf(val, sizeof(val), "[%3d]", (int)(value * 100.0f));
    hud_phosphor_text(ctx, val, px + 12.0f, py + fs * 0.35f, fs, col);
}

/* ============================================================
 * Per-core readout grid: [NN] NN% rows, bracketed digital style
 *
 *   CORES
 *   [00] 19    [01] 22    [02] 05    [03] 67
 *   [04] 18    [05] 30    [06] 82    [07] 14
 *
 * Each core is rendered as an ETCHED bracketed index followed by
 * a PROJECTED percentage value colored by usage.
 * ============================================================ */

static void draw_core_strip(vgfx_ctx_t *ctx, float x, float y, float w, float h)
{
    float label_fs = 12.0f;
    float cell_fs = 12.0f;

    /* ETCHED section label */
    vgfx_text(ctx, "CORES", x + 0.6f, y + label_fs + 0.6f, label_fs,
               vgfx_rgba(0, 0, 0, 0.5f));
    vgfx_text_bold(ctx, "CORES", x, y + label_fs, label_fs, C_DIM);

    if (D.num_cores <= 0) return;

    /* 4-column grid */
    int cols = 4;
    float row_h = 18.0f;
    float grid_top = y + label_fs * 2.0f;
    float cell_w = w / (float)cols;
    (void)h;

    for (int i = 0; i < D.num_cores; i++) {
        int r = i / cols;
        int c = i % cols;
        if ((float)(r + 1) * row_h > h - label_fs * 2.0f) break;

        float cx = x + (float)c * cell_w;
        float cy = grid_top + (float)r * row_h;
        float v = D.core[i];
        vgfx_color_t col = usage_color(v);

        /* Bracketed index ETCHED */
        char idx[8]; snprintf(idx, sizeof(idx), "[%02d]", i);
        vgfx_text(ctx, idx, cx + 0.5f, cy + cell_fs + 0.5f, cell_fs,
                   vgfx_rgba(0, 0, 0, 0.5f));
        vgfx_text(ctx, idx, cx, cy + cell_fs, cell_fs, C_DIM);

        /* PROJECTED value -- phosphor glow */
        char val[8]; snprintf(val, sizeof(val), "%3d", (int)(v * 100.0f));
        hud_phosphor_text(ctx, val, cx + 40.0f, cy + cell_fs, cell_fs, col);

        /* Tiny inline bar: static baseline + dynamic pointer */
        float bar_x = cx + 70.0f;
        float bar_w = cell_w - 80.0f;
        if (bar_w > 10.0f) {
            vgfx_line(ctx, bar_x, cy + cell_fs * 0.65f,
                             bar_x + bar_w, cy + cell_fs * 0.65f,
                             1.0f, C_DIM);
            float mx = bar_x + bar_w * v;
            hud_phosphor_line(ctx, mx, cy + cell_fs * 0.35f,
                                 mx, cy + cell_fs * 0.95f, 1.4f, col);
        }
    }
}

/* ============================================================
 * Data field: LABEL  [VALUE]
 * Bracketed digital readout style. Label ETCHED, value PROJECTED.
 * ============================================================ */

static void draw_field(vgfx_ctx_t *ctx, float x, float y, float label_w,
                         const char *label, const char *value,
                         vgfx_color_t value_color)
{
    float fs = 13.0f;
    /* ETCHED label -- static, black */
    vgfx_text(ctx, label, x + 0.5f, y + fs, fs, vgfx_rgba(1, 1, 1, 0.15f));
    vgfx_text(ctx, label, x, y + fs, fs, C_DIM);

    /* Bracketed PROJECTED value -- phosphor glow */
    char boxed[128];
    snprintf(boxed, sizeof(boxed), "[%s]", value);
    hud_phosphor_text(ctx, boxed, x + label_w, y + fs, fs, value_color);
}

/* ============================================================
 * Main render
 * ============================================================ */

static void render(vgfx_ctx_t *ctx)
{
    static int tick = 0;
    if (++tick >= 60) { sample(); tick = 0; }

    init_colors();
    float w = ctx->width, h = ctx->height;
    float pad = 16.0f;

    vgfx_clear(ctx, C_BG);

    /* ============================================================
     * Top row: 4 bar gauges (CPU, MEM, GPU, SWP)
     * ============================================================ */
    float gauge_y = pad;
    float gauge_h = h * 0.42f;
    float gauge_w = (w - pad * 5) / 4.0f;

    float gx = pad;
    draw_bar_gauge(ctx, gx, gauge_y, gauge_w, gauge_h, "CPU", D.cpu_total);
    gx += gauge_w + pad;
    draw_bar_gauge(ctx, gx, gauge_y, gauge_w, gauge_h, "MEM", D.mem_pct);
    gx += gauge_w + pad;
    draw_bar_gauge(ctx, gx, gauge_y, gauge_w, gauge_h, "GPU", D.gpu_ok ? D.gpu_pct : 0);
    gx += gauge_w + pad;
    draw_bar_gauge(ctx, gx, gauge_y, gauge_w, gauge_h, "SWP", D.swap_pct);

    /* ============================================================
     * Divider
     * ============================================================ */
    float div1_y = gauge_y + gauge_h + pad;
    vgfx_line(ctx, pad, div1_y, w - pad, div1_y, 1.0f, C_DIM);

    /* ============================================================
     * Middle: per-core bars
     * ============================================================ */
    float core_y = div1_y + pad;
    float core_h = 90.0f;
    draw_core_strip(ctx, pad, core_y, w - pad * 2, core_h);

    /* ============================================================
     * Divider
     * ============================================================ */
    float div2_y = core_y + core_h + pad * 0.5f;
    vgfx_line(ctx, pad, div2_y, w - pad, div2_y, 1.0f, C_DIM);

    /* ============================================================
     * Bottom: data fields
     * ============================================================ */
    float field_y = div2_y + pad;
    float row_h = 20.0f;
    float col_w = (w - pad * 3) / 2.0f;

    /* Left column: system identification */
    {
        char buf[64];
        float fy = field_y;
        draw_field(ctx, pad, fy, 90, "HOST", D.hostname[0] ? D.hostname : "--", C_HI); fy += row_h;
        draw_field(ctx, pad, fy, 90, "KERNEL", D.kernel[0] ? D.kernel : "--", C_FG); fy += row_h;

        int uh = (int)(D.uptime / 3600);
        int um = (int)(D.uptime / 60) % 60;
        int ud = uh / 24;
        uh = uh % 24;
        snprintf(buf, sizeof(buf), "%dd %02dh %02dm", ud, uh, um);
        draw_field(ctx, pad, fy, 90, "UPTIME", buf, C_FG); fy += row_h;

        snprintf(buf, sizeof(buf), "%d", D.procs);
        draw_field(ctx, pad, fy, 90, "PROCS", buf, C_FG); fy += row_h;

        snprintf(buf, sizeof(buf), "%.2f  %.2f  %.2f", D.load[0], D.load[1], D.load[2]);
        draw_field(ctx, pad, fy, 90, "LOAD", buf, C_FG); fy += row_h;
    }

    /* Right column: I/O */
    {
        char buf[64];
        float rx = pad * 2 + col_w;
        float fy = field_y;

        /* Memory detail */
        if (D.mem_total_kb > 0) {
            float used_gb = (float)(D.mem_total_kb - D.mem_avail_kb) / 1048576.0f;
            float total_gb = (float)D.mem_total_kb / 1048576.0f;
            snprintf(buf, sizeof(buf), "%.1f / %.1f GB", used_gb, total_gb);
            draw_field(ctx, rx, fy, 90, "RAM", buf, C_FG); fy += row_h;
        }
        if (D.swap_total_kb > 0) {
            float used_gb = (float)(D.swap_total_kb - D.swap_free_kb) / 1048576.0f;
            float total_gb = (float)D.swap_total_kb / 1048576.0f;
            snprintf(buf, sizeof(buf), "%.1f / %.1f GB", used_gb, total_gb);
            draw_field(ctx, rx, fy, 90, "SWAP", buf, C_FG); fy += row_h;
        }

        /* Network rate */
        double rx_kb = (double)D.rx_bytes_per_sec / 1024.0;
        double tx_kb = (double)D.tx_bytes_per_sec / 1024.0;
        snprintf(buf, sizeof(buf), "%.1f KB/s", rx_kb);
        draw_field(ctx, rx, fy, 90, "NET RX", buf, C_FG); fy += row_h;
        snprintf(buf, sizeof(buf), "%.1f KB/s", tx_kb);
        draw_field(ctx, rx, fy, 90, "NET TX", buf, C_FG); fy += row_h;

        /* Disk I/O */
        snprintf(buf, sizeof(buf), "R %ld  W %ld",
                 D.disk_reads_per_sec, D.disk_writes_per_sec);
        draw_field(ctx, rx, fy, 90, "DISK", buf, C_FG); fy += row_h;
    }

    /* Bottom status line -- ETCHED */
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char s[32];
        snprintf(s, sizeof(s), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
        vgfx_text(ctx, "VGP SYS", pad, h - 6, 10, C_DIM);
        float tw = vgfx_text_width(ctx, s, -1, 10);
        vgfx_text(ctx, s, w - pad - tw, h - 6, 10, C_DIM);
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    sample();

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP System Monitor", 900, 600, 0) < 0) return 1;
    vgfx_run_animated(&ctx, render, sample, 1000);
    vgfx_destroy(&ctx);
    return 0;
}
