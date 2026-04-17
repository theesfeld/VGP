/* VGP Bar -- HUD-style status strip.
 * Each widget is a boxed ETCHED label + PROJECTED value.
 * Transparent background; the compositor's plexiglass sits underneath. */

#include "vgp-gfx.h"
#include "vgp-hud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_WIDGETS 16
#define WIDGET_BUF  128

typedef struct {
    char name[32];
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
    snprintf(w->name, sizeof(w->name), "WS");
    snprintf(w->output, WIDGET_BUF, "1 2 3");
    w->align = 'l';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "HOST");
    snprintf(w->command, sizeof(w->command), "hostname -s");
    w->interval = 60; w->align = 'c';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "CPU");
    snprintf(w->command, sizeof(w->command),
             "awk '/^cpu /{printf \"%%.0f\", 100-($5/($2+$3+$4+$5+$6+$7+$8)*100)}' /proc/stat");
    w->interval = 3; w->align = 'r';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "CLK");
    snprintf(w->command, sizeof(w->command), "date '+%%H:%%M'");
    w->interval = 30; w->align = 'r';

    for (int i = 0; i < bar.count; i++) update_widget(&bar.widgets[i]);
}

static void render(vgfx_ctx_t *ctx)
{
    hud_palette_t P = hud_palette();

    /* Update widget timers */
    for (int i = 0; i < bar.count; i++) {
        bar_widget_t *w = &bar.widgets[i];
        if (w->interval > 0) {
            w->timer--;
            if (w->timer <= 0) {
                update_widget(w);
                w->timer = w->interval * 2;
            }
        }
    }

    vgfx_clear(ctx, vgfx_rgba(0, 0, 0, 0));

    float fs = 12.0f;
    float h = ctx->height;
    float w = ctx->width;
    float pad = 8.0f;

    /* Top accent bezel */
    vgfx_line(ctx, 0, 0, w, 0, 1.0f, P.warn);

    float left_x = pad;
    float right_x = w - pad;

    for (int i = 0; i < bar.count; i++) {
        bar_widget_t *wg = &bar.widgets[i];
        if (!wg->output[0]) continue;

        char item[WIDGET_BUF + 32];
        snprintf(item, sizeof(item), "%s %s", wg->name, wg->output);
        float tw = vgfx_text_width(ctx, item, -1, fs);
        float box_w = tw + 16.0f;
        float box_h = h - 6.0f;
        float box_y = 3.0f;

        float bx;
        if (wg->align == 'l') {
            bx = left_x;
            left_x += box_w + pad;
        } else if (wg->align == 'r') {
            right_x -= box_w;
            bx = right_x;
            right_x -= pad;
        } else {
            bx = (w - box_w) * 0.5f;
        }

        /* Boxed section with ETCHED label and PROJECTED value */
        vgfx_rect_outline(ctx, bx, box_y, box_w, box_h, 1.0f, P.dim);
        hud_etched(ctx, wg->name, bx + 6,
                    box_y + box_h * 0.5f + fs * 0.35f, fs - 2, &P);
        float lw = vgfx_text_width(ctx, wg->name, -1, fs - 2);
        vgfx_text_bold(ctx, wg->output, bx + 6 + lw + 6,
                        box_y + box_h * 0.5f + fs * 0.35f, fs,
                        wg->align == 'c' ? P.warn : P.fg);
    }
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    load_config();

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Bar", 1920, 28, VGP_WINDOW_OVERRIDE) < 0)
        return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
