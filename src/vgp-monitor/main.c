/* VGP Tactical System Monitor -- Fighter Jet HUD
 * 3D wireframe sphere, per-core CPU, GPU, horizon indicator,
 * tape scales, trace waveforms. NERV/F-16 aesthetic.
 * Renders at 60fps with continuous animation. */

#include "vgp-gfx.h"
#include "vgp-gfx-3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* ============================================================
 * Data
 * ============================================================ */

#define MAX_CORES 64
#define HIST 200

static struct {
    /* Per-core CPU */
    float core[MAX_CORES];
    long  core_prev_idle[MAX_CORES];
    long  core_prev_total[MAX_CORES];
    int   num_cores;
    float cpu_total;
    float cpu_hist[HIST];

    /* Memory */
    long  mem_total_kb, mem_avail_kb;
    long  swap_total_kb, swap_free_kb;
    float mem_pct, swap_pct;
    float mem_hist[HIST];

    /* GPU */
    float gpu_pct;
    float gpu_hist[HIST];
    char  gpu_name[64];
    bool  gpu_ok;

    /* Network */
    long  prev_rx, prev_tx;
    float net_rx, net_tx; /* normalized 0-1 */
    float net_rx_hist[HIST];

    /* System */
    float uptime;
    float load[3];
    int   procs;
    char  hostname[64];

    int   head;
    float t; /* animation time */
} D;

static void sample(void)
{
    /* Per-core CPU */
    FILE *f = fopen("/proc/stat", "r");
    if (f) {
        char line[256];
        int core_idx = -1;
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu", 3) != 0) break;
            long u,n,s,id,io,ir,si,st;
            if (line[3] == ' ') { /* aggregate */
                if (sscanf(line+4, "%ld %ld %ld %ld %ld %ld %ld %ld", &u,&n,&s,&id,&io,&ir,&si,&st) == 8) {
                    long total = u+n+s+id+io+ir+si+st;
                    long di = id - D.core_prev_idle[0];
                    long dt = total - D.core_prev_total[0];
                    if (dt > 0) D.cpu_total = (float)(dt-di)/(float)dt;
                    D.core_prev_idle[0] = id; D.core_prev_total[0] = total;
                }
                continue;
            }
            core_idx++;
            if (core_idx >= MAX_CORES) continue;
            int ci = 0;
            if (sscanf(line, "cpu%d %ld %ld %ld %ld %ld %ld %ld %ld", &ci, &u,&n,&s,&id,&io,&ir,&si,&st) == 9) {
                long total = u+n+s+id+io+ir+si+st;
                int idx = ci + 1; /* offset: [0] is aggregate */
                if (idx < MAX_CORES) {
                    long di = id - D.core_prev_idle[idx];
                    long dt = total - D.core_prev_total[idx];
                    if (dt > 0) D.core[ci] = (float)(dt-di)/(float)dt;
                    D.core_prev_idle[idx] = id; D.core_prev_total[idx] = total;
                }
                if (ci >= D.num_cores) D.num_cores = ci + 1;
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
    /* Try AMD */
    f = fopen("/sys/class/drm/card0/device/gpu_busy_percent", "r");
    if (!f) f = fopen("/sys/class/drm/card1/device/gpu_busy_percent", "r");
    if (f) { int g; if (fscanf(f,"%d",&g)==1) { D.gpu_pct=(float)g/100.0f; D.gpu_ok=true; } fclose(f); }
    /* Try Intel */
    if (!D.gpu_ok) {
        f = fopen("/sys/class/drm/card0/gt/gt_cur_freq_mhz", "r");
        if (f) {
            int cur = 0; fscanf(f,"%d",&cur); fclose(f);
            f = fopen("/sys/class/drm/card0/gt/gt_max_freq_mhz", "r");
            if (f) { int mx = 0; fscanf(f,"%d",&mx); fclose(f);
                if (mx > 0) { D.gpu_pct = (float)cur/(float)mx; D.gpu_ok = true; } }
        }
    }

    /* Network */
    f = fopen("/proc/net/dev", "r");
    if (f) {
        char line[512];
        fgets(line,sizeof(line),f); fgets(line,sizeof(line),f);
        while (fgets(line,sizeof(line),f)) {
            char iface[32]; long rx=0, tx=0;
            if (sscanf(line," %31[^:]:%ld %*d %*d %*d %*d %*d %*d %*d %ld", iface,&rx,&tx)>=3) {
                char *p = iface; while(*p==' ') p++;
                if (strcmp(p,"lo")==0) continue;
                if (D.prev_rx > 0) {
                    long drx = rx-D.prev_rx, dtx = tx-D.prev_tx;
                    D.net_rx = (float)drx/1000000.0f; if (D.net_rx>1) D.net_rx=1;
                    D.net_tx = (float)dtx/1000000.0f; if (D.net_tx>1) D.net_tx=1;
                }
                D.prev_rx=rx; D.prev_tx=tx;
                break;
            }
        }
        fclose(f);
    }

    /* System */
    f = fopen("/proc/uptime","r");
    if (f) { fscanf(f,"%f",&D.uptime); fclose(f); }
    f = fopen("/proc/loadavg","r");
    if (f) { fscanf(f,"%f %f %f",&D.load[0],&D.load[1],&D.load[2]); fclose(f); }
    if (!D.hostname[0]) gethostname(D.hostname, sizeof(D.hostname));
    f = popen("ls -1d /proc/[0-9]* 2>/dev/null | wc -l","r");
    if (f) { fscanf(f,"%d",&D.procs); pclose(f); }

    /* History */
    D.cpu_hist[D.head] = D.cpu_total;
    D.mem_hist[D.head] = D.mem_pct;
    D.gpu_hist[D.head] = D.gpu_pct;
    D.net_rx_hist[D.head] = D.net_rx;
    D.head = (D.head + 1) % HIST;
}

/* ============================================================
 * HUD Colors
 * ============================================================ */

static vgfx_color_t HG, HD, HB, HW, HR, HBG;

static void init_colors(void) {
    HG  = vgfx_rgba(0.0f, 0.85f, 0.0f, 0.9f);  /* green */
    HD  = vgfx_rgba(0.0f, 0.35f, 0.0f, 0.5f);  /* dim green */
    HB  = vgfx_rgba(0.0f, 1.0f, 0.0f, 1.0f);   /* bright green */
    HW  = vgfx_rgba(1.0f, 0.85f, 0.0f, 1.0f);  /* warning yellow */
    HR  = vgfx_rgba(1.0f, 0.2f, 0.2f, 1.0f);   /* red/critical */
    HBG = vgfx_rgba(0.0f, 0.02f, 0.0f, 1.0f);  /* near-black */
}

static vgfx_color_t usage_color(float v) {
    if (v > 0.9f) return HR;
    if (v > 0.75f) return HW;
    return HG;
}

/* ============================================================
 * HUD Elements
 * ============================================================ */

/* Scanlines */
static void draw_scanlines(vgfx_ctx_t *c, float w, float h)
{
    for (float y = 0; y < h; y += 3)
        vgfx_line(c, 0, y, w, y, 0.3f, vgfx_rgba(0, 0.04f, 0, 0.2f));
}

/* Corner brackets */
static void draw_frame(vgfx_ctx_t *c, float w, float h)
{
    float s = 25, lw = 1.5f;
    vgfx_line(c,2,2,2+s,2,lw,HG); vgfx_line(c,2,2,2,2+s,lw,HG);
    vgfx_line(c,w-2,2,w-2-s,2,lw,HG); vgfx_line(c,w-2,2,w-2,2+s,lw,HG);
    vgfx_line(c,2,h-2,2+s,h-2,lw,HG); vgfx_line(c,2,h-2,2,h-2-s,lw,HG);
    vgfx_line(c,w-2,h-2,w-2-s,h-2,lw,HG); vgfx_line(c,w-2,h-2,w-2,h-2-s,lw,HG);
}

/* Vertical tape scale */
static void draw_tape(vgfx_ctx_t *c, float x, float y, float w, float h,
                        float val, const char *label, bool right)
{
    float fs = 10, lw = 1.0f;
    vgfx_rect_outline(c, x, y, w, h, lw, HG);
    vgfx_text_bold(c, label, x+2, y-2, fs, HG);

    /* Value box */
    float bh = 18, by = y + h*0.5f - bh*0.5f;
    char val_s[16]; snprintf(val_s, sizeof(val_s), "%3.0f", val);
    if (right) {
        vgfx_rect(c, x+w, by, 36, bh, HBG);
        vgfx_rect_outline(c, x+w, by, 36, bh, lw, HB);
        vgfx_text_bold(c, val_s, x+w+3, by+14, fs, HB);
    } else {
        vgfx_rect(c, x-36, by, 36, bh, HBG);
        vgfx_rect_outline(c, x-36, by, 36, bh, lw, HB);
        vgfx_text_bold(c, val_s, x-33, by+14, fs, HB);
    }

    /* Scrolling ticks */
    vgfx_push_clip(c, x+1, y+1, w-2, h-2);
    float cy = y + h*0.5f;
    float ppu = h / 50.0f;
    for (int v = (int)(val-25); v <= (int)(val+25); v++) {
        if (v < 0 || v > 100) continue;
        float ty = cy - ((float)v - val) * ppu;
        bool major = (v%10==0);
        float tw = major ? w*0.4f : w*0.15f;
        vgfx_color_t tc = (v >= 80) ? HW : HG;
        if (right) vgfx_line(c, x, ty, x+tw, ty, major?1.2f:0.6f, tc);
        else vgfx_line(c, x+w-tw, ty, x+w, ty, major?1.2f:0.6f, tc);
        if (major) {
            char n[8]; snprintf(n,sizeof(n),"%d",v);
            float tx = right ? x+tw+2 : x+w-tw-20;
            vgfx_text(c, n, tx, ty+4, fs-2, tc);
        }
    }
    vgfx_pop_clip(c);
}

/* Trace waveform */
static void draw_trace(vgfx_ctx_t *c, float x, float y, float w, float h,
                          float *data, int head, vgfx_color_t col, const char *label)
{
    vgfx_rect_outline(c, x, y, w, h, 0.6f, HD);
    vgfx_text(c, label, x+3, y+10, 8, col);
    for (int i = 1; i < 4; i++)
        vgfx_line(c, x, y+h*(float)i/4, x+w, y+h*(float)i/4, 0.3f, HD);
    float step = w / (float)(HIST-1);
    for (int i = 1; i < HIST; i++) {
        int i0 = (head+i-1)%HIST, i1 = (head+i)%HIST;
        vgfx_line(c, x+(float)(i-1)*step, y+h*(1-data[i0]),
                    x+(float)i*step, y+h*(1-data[i1]), 1.0f, col);
    }
}

/* Per-core bars */
static void draw_core_bars(vgfx_ctx_t *c, float x, float y, float w, float h)
{
    if (D.num_cores <= 0) return;
    float bar_w = w / (float)D.num_cores - 2;
    if (bar_w < 2) bar_w = 2;
    for (int i = 0; i < D.num_cores; i++) {
        float bx = x + (float)i * (bar_w + 2);
        vgfx_rect_outline(c, bx, y, bar_w, h, 0.5f, HD);
        float fill = D.core[i] * h;
        vgfx_rect(c, bx, y+h-fill, bar_w, fill, usage_color(D.core[i]));
        if (bar_w >= 6) {
            char n[4]; snprintf(n,sizeof(n),"%d",i);
            vgfx_text(c, n, bx+1, y+h+9, 7, HD);
        }
    }
}

/* ============================================================
 * Main HUD render
 * ============================================================ */

static vgfx_mesh_t *sphere = NULL;
static float sphere_angle = 0;

static void render(vgfx_ctx_t *ctx)
{
    D.t += 0.016f;
    float w = ctx->width, h = ctx->height;

    init_colors();
    vgfx_clear(ctx, HBG);
    draw_scanlines(ctx, w, h);
    draw_frame(ctx, w, h);

    /* Create sphere once */
    if (!sphere) sphere = vgfx_mesh_sphere(12, 24, 1.0f);

    /* Top info */
    {
        int uh = (int)(D.uptime/3600), um = (int)(D.uptime/60)%60;
        char info[128];
        snprintf(info, sizeof(info), "%s  UP %02d:%02d  PRC %d  LOAD %.1f %.1f %.1f",
                 D.hostname, uh, um, D.procs, D.load[0], D.load[1], D.load[2]);
        float tw = vgfx_text_width(ctx, info, -1, 10);
        vgfx_text(ctx, info, w*0.5f - tw*0.5f, 16, 10, HG);
    }

    /* Tapes */
    float tape_h = h * 0.5f;
    draw_tape(ctx, 8, 30, 28, tape_h, D.cpu_total*100, "CPU", false);
    draw_tape(ctx, w-36, 30, 28, tape_h, D.mem_pct*100, "MEM", true);

    /* 3D Wireframe Sphere -- center */
    {
        float cx = w * 0.5f, cy = h * 0.3f;
        float r = fminf(w, h) * 0.15f;

        vgfx_3d_ctx_t ctx3d;
        vgfx_3d_init(&ctx3d, cx, cy, r, 45.0f);

        /* Rotation speed proportional to CPU load */
        sphere_angle += 0.005f + D.cpu_total * 0.03f;
        float wobble = sinf(D.t * 0.4f) * 0.15f;

        vgfx_mat4_t model = vgfx_mat4_multiply(
            vgfx_mat4_rotate_y(sphere_angle),
            vgfx_mat4_rotate_x(wobble));
        vgfx_3d_set_model(&ctx3d, model);

        /* Color edges by latitude band -> per-core CPU */
        if (sphere) {
            vgfx_color_t *colors = malloc(sizeof(vgfx_color_t) * (size_t)sphere->edge_count);
            if (colors) {
                for (int i = 0; i < sphere->edge_count; i++) {
                    /* Determine which latitude band this edge belongs to */
                    vgfx_vec3_t va = sphere->verts[sphere->edges[i].a];
                    vgfx_vec3_t vb = sphere->verts[sphere->edges[i].b];
                    float avg_y = (va.y + vb.y) * 0.5f;
                    /* Map Y position (-1..1) to core index */
                    int core_idx = (int)((avg_y + 1.0f) * 0.5f * (float)D.num_cores);
                    if (core_idx < 0) core_idx = 0;
                    if (core_idx >= D.num_cores) core_idx = D.num_cores - 1;
                    float usage = (D.num_cores > 0) ? D.core[core_idx] : D.cpu_total;
                    colors[i] = usage_color(usage);
                    colors[i].a *= (0.5f + usage * 0.5f); /* brighter with load */
                }
                vgfx_draw_mesh_colored(ctx, &ctx3d, sphere, 1.0f, colors);
                free(colors);
            }
        }
    }

    /* Horizon indicator */
    {
        float cx = w * 0.5f, cy = h * 0.55f;
        float r = fminf(w, h) * 0.1f;
        float pitch = (D.cpu_total - 0.5f) * 30.0f * M_PI / 180.0f;
        float roll = (D.load[0] - D.load[2]) * 5.0f * M_PI / 180.0f;

        /* Circle border */
        int seg = 36;
        for (int i = 0; i < seg; i++) {
            float a0 = (float)i/(float)seg * 2*M_PI;
            float a1 = (float)(i+1)/(float)seg * 2*M_PI;
            vgfx_line(ctx, cx+r*cosf(a0), cy+r*sinf(a0),
                        cx+r*cosf(a1), cy+r*sinf(a1), 1.0f, HG);
        }

        /* Horizon line (rotated) */
        float cos_r = cosf(roll), sin_r = sinf(roll);
        float offset = pitch * r * 2;
        float hx1 = -r * 1.2f, hx2 = r * 1.2f;
        float hy = offset;
        /* Rotate */
        float rx1 = hx1*cos_r - hy*sin_r, ry1 = hx1*sin_r + hy*cos_r;
        float rx2 = hx2*cos_r - hy*sin_r, ry2 = hx2*sin_r + hy*cos_r;
        vgfx_push_clip(ctx, cx-r, cy-r, r*2, r*2);
        vgfx_line(ctx, cx+rx1, cy+ry1, cx+rx2, cy+ry2, 1.5f, HB);

        /* Pitch ladder */
        for (int d = -15; d <= 15; d += 5) {
            if (d == 0) continue;
            float py = offset + (float)d / 15.0f * r;
            float pw = r * 0.3f;
            float lx1 = -pw, lx2 = pw, ly = py;
            float rlx1 = lx1*cos_r - ly*sin_r, rly1 = lx1*sin_r + ly*cos_r;
            float rlx2 = lx2*cos_r - ly*sin_r, rly2 = lx2*sin_r + ly*cos_r;
            vgfx_line(ctx, cx+rlx1, cy+rly1, cx+rlx2, cy+rly2, 0.8f, HG);
        }
        vgfx_pop_clip(ctx);

        /* Aircraft symbol */
        vgfx_line(ctx, cx-15, cy, cx-5, cy, 1.5f, HB);
        vgfx_line(ctx, cx+5, cy, cx+15, cy, 1.5f, HB);
        vgfx_line(ctx, cx, cy-5, cx, cy+5, 1.5f, HB);

        /* Label */
        vgfx_text(ctx, "ATT", cx-10, cy+r+14, 8, HD);
    }

    /* Per-core bars */
    {
        float bar_y = h * 0.68f;
        float bar_h = h * 0.06f;
        float bar_x = 55, bar_w = w - 110;
        vgfx_text(ctx, "CORES", bar_x, bar_y - 4, 8, HD);
        draw_core_bars(ctx, bar_x, bar_y, bar_w, bar_h);
    }

    /* Traces */
    {
        float ty = h * 0.77f;
        float th = h * 0.08f;
        float tw = w * 0.42f;

        draw_trace(ctx, 55, ty, tw, th, D.cpu_hist, D.head, HG, "CPU");
        draw_trace(ctx, w-55-tw, ty, tw, th, D.mem_hist, D.head, HB, "MEM");

        float ty2 = ty + th + 6;
        float th2 = h * 0.06f;
        draw_trace(ctx, 55, ty2, tw, th2, D.gpu_hist, D.head,
                    vgfx_rgba(0.8f,0.4f,0.0f,0.8f), "GPU");
        draw_trace(ctx, w-55-tw, ty2, tw, th2, D.net_rx_hist, D.head,
                    vgfx_rgba(0.0f,0.6f,1.0f,0.8f), "NET");
    }

    /* Bottom detail */
    {
        float by = h - 30;
        float fs = 9;
        char buf[128];
        snprintf(buf, sizeof(buf), "GPU: %s %.0f%%",
                 D.gpu_ok ? "OK" : "--", D.gpu_pct*100);
        vgfx_text(ctx, buf, 55, by, fs, D.gpu_ok ? HG : HD);

        snprintf(buf, sizeof(buf), "RAM: %.1f/%.1f GB  SWAP: %.1f/%.1f GB",
                 (float)(D.mem_total_kb - D.mem_avail_kb)/1048576.0f,
                 (float)D.mem_total_kb/1048576.0f,
                 (float)(D.swap_total_kb - D.swap_free_kb)/1048576.0f,
                 (float)D.swap_total_kb/1048576.0f);
        float tw = vgfx_text_width(ctx, buf, -1, fs);
        vgfx_text(ctx, buf, w - 55 - tw, by, fs, HG);
    }

    /* Bottom status */
    {
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char s[64];
        snprintf(s, sizeof(s), "VGP HUD  %02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
        vgfx_text(ctx, s, 55, h - 10, 8, HD);
        /* Blink */
        if ((int)(D.t * 2) % 2 == 0)
            vgfx_circle(ctx, w - 20, h - 10, 3, HG);
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Tactical Monitor", 850, 650, 0) < 0) return 1;
    vgfx_run_animated(&ctx, render, sample, 1000);
    if (sphere) vgfx_mesh_destroy(sphere);
    vgfx_destroy(&ctx);
    return 0;
}
