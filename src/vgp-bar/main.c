/* VGP Bar -- GPU-rendered standalone scriptable status bar */

#include "vgp-gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_WIDGETS 16
#define WIDGET_BUF 256

typedef struct {
    char name[64];
    char command[256];
    char output[WIDGET_BUF];
    int  interval;
    int  timer;
    char align;
} bar_widget_t;

static struct {
    bar_widget_t widgets[MAX_WIDGETS];
    int          count;
} bar;

static void update_widget(bar_widget_t *w)
{
    if (!w->command[0]) return;
    FILE *p = popen(w->command, "r");
    if (!p) return;
    if (fgets(w->output, WIDGET_BUF, p)) {
        size_t len = strlen(w->output);
        while (len > 0 && (w->output[len-1] == '\n' || w->output[len-1] == '\r'))
            w->output[--len] = '\0';
    }
    pclose(p);
}

static void load_config(void)
{
    bar.count = 0;
    bar_widget_t *w;

    w = &bar.widgets[bar.count++];
    snprintf(w->name, 64, "workspaces");
    snprintf(w->output, WIDGET_BUF, "[1] [2] [3]");
    w->align = 'l';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, 64, "title");
    snprintf(w->output, WIDGET_BUF, "VGP Bar");
    w->align = 'c';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, 64, "clock");
    snprintf(w->command, 256, "date '+%%H:%%M'");
    w->interval = 30; w->align = 'r';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, 64, "cpu");
    snprintf(w->command, 256,
             "awk '/^cpu /{printf \"%%.0f%%%%\", 100-($5/($2+$3+$4+$5+$6+$7+$8)*100)}' /proc/stat");
    w->interval = 5; w->align = 'r';

    for (int i = 0; i < bar.count; i++) update_widget(&bar.widgets[i]);
}

static void render(vgfx_ctx_t *ctx)
{
    /* Tick widget timers */
    for (int i = 0; i < bar.count; i++) {
        bar_widget_t *w = &bar.widgets[i];
        if (w->interval > 0) {
            w->timer--;
            if (w->timer <= 0) { update_widget(w); w->timer = w->interval * 2; }
        }
    }

    vgfx_clear(ctx, vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));

    float fs = ctx->theme.font_size;
    float h = ctx->height;
    float w = ctx->width;
    float ty = h * 0.5f + fs * 0.35f;
    float pad = 8;

    /* Top border */
    vgfx_line(ctx, 0, 0, w, 0, 1, vgfx_theme_color(ctx, VGP_THEME_ACCENT));

    float left_x = pad;
    float right_x = w - pad;

    for (int i = 0; i < bar.count; i++) {
        bar_widget_t *wg = &bar.widgets[i];
        float tw = vgfx_text_width(ctx, wg->output, -1, fs);

        if (wg->align == 'l') {
            vgfx_text(ctx, wg->output, left_x, ty, fs, vgfx_theme_color(ctx, VGP_THEME_FG));
            left_x += tw + pad * 2;
        } else if (wg->align == 'r') {
            right_x -= tw;
            vgfx_text(ctx, wg->output, right_x, ty, fs, vgfx_theme_color(ctx, VGP_THEME_FG));
            right_x -= pad * 2;
        } else {
            float cx = (w - tw) * 0.5f;
            vgfx_text_bold(ctx, wg->output, cx, ty, fs, vgfx_theme_color(ctx, VGP_THEME_ACCENT));
        }
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    load_config();

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Bar", 1920, 28, VGP_WINDOW_OVERRIDE) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
