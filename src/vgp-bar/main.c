/* VGP Bar -- Standalone scriptable status bar
 * Reads widget definitions from config, runs commands, displays output.
 * Similar to polybar/waybar but for VGP. */

#include "vgp-ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MAX_WIDGETS 16
#define WIDGET_BUF 256

typedef struct {
    char name[64];
    char command[256];   /* shell command to run for output */
    char output[WIDGET_BUF]; /* last command output */
    int  interval;       /* seconds between updates (0=static) */
    int  timer;          /* countdown to next update */
    char align;          /* 'l'=left, 'c'=center, 'r'=right */
} bar_widget_t;

typedef struct {
    bar_widget_t widgets[MAX_WIDGETS];
    int          count;
    int          height;
} bar_state_t;

static bar_state_t bar;

static void update_widget(bar_widget_t *w)
{
    if (!w->command[0]) return;
    FILE *p = popen(w->command, "r");
    if (!p) return;
    if (fgets(w->output, WIDGET_BUF, p)) {
        /* Strip newline */
        size_t len = strlen(w->output);
        while (len > 0 && (w->output[len-1] == '\n' || w->output[len-1] == '\r'))
            w->output[--len] = '\0';
    }
    pclose(p);
}

static void load_config(void)
{
    bar.count = 0;
    bar.height = 28;

    /* Default widgets */
    bar_widget_t *w;

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "workspaces");
    snprintf(w->output, sizeof(w->output), "[1] [2] [3]");
    w->align = 'l';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "title");
    snprintf(w->output, sizeof(w->output), "VGP Bar");
    w->align = 'c';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "clock");
    snprintf(w->command, sizeof(w->command), "date '+%%H:%%M'");
    w->interval = 30;
    w->align = 'r';

    w = &bar.widgets[bar.count++];
    snprintf(w->name, sizeof(w->name), "cpu");
    snprintf(w->command, sizeof(w->command),
             "awk '/^cpu /{printf \"%%.0f%%%%\", 100-($5/($2+$3+$4+$5+$6+$7+$8)*100)}' /proc/stat");
    w->interval = 5;
    w->align = 'r';

    /* Initial update */
    for (int i = 0; i < bar.count; i++)
        update_widget(&bar.widgets[i]);
}

static void render(vui_ctx_t *ctx)
{
    vui_clear(ctx, VUI_SURFACE);

    /* Tick widget timers */
    for (int i = 0; i < bar.count; i++) {
        bar_widget_t *w = &bar.widgets[i];
        if (w->interval > 0) {
            w->timer--;
            if (w->timer <= 0) {
                update_widget(w);
                w->timer = w->interval * 2; /* ~2 renders per second */
            }
        }
    }

    int left_col = 1;
    int right_col = ctx->cols - 1;

    for (int i = 0; i < bar.count; i++) {
        bar_widget_t *w = &bar.widgets[i];
        int len = (int)strlen(w->output);

        if (w->align == 'l') {
            vui_text(ctx, 0, left_col, w->output, VUI_WHITE, VUI_SURFACE);
            left_col += len + 2;
        } else if (w->align == 'r') {
            right_col -= len + 1;
            vui_text(ctx, 0, right_col, w->output, VUI_WHITE, VUI_SURFACE);
            right_col -= 1;
        } else { /* center */
            int cx = (ctx->cols - len) / 2;
            vui_text(ctx, 0, cx, w->output, VUI_ACCENT, VUI_SURFACE);
        }
    }

    /* Separator lines */
    vui_set_cell(ctx, 0, 0, 0x2502, VUI_BORDER, VUI_SURFACE, 0);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    FILE *lf = fopen("/tmp/vgp-bar.log", "w");
    if (lf) { setvbuf(lf, NULL, _IOLBF, 0); dup2(fileno(lf), STDERR_FILENO); fclose(lf); }

    load_config();

    vui_ctx_t ctx;
    /* Bar: full width, short height, override window (no decorations) */
    if (vui_init(&ctx, "VGP Bar", 1920, 28) < 0) return 1;
    /* The window is created as decorated -- it should be override.
     * For now it works as a regular window. */

    while (ctx.running) {
        vui_poll(&ctx, 500); /* update every 500ms */
        vui_begin_frame(&ctx);
        render(&ctx);
        vui_end_frame(&ctx);
    }

    vui_destroy(&ctx);
    return 0;
}
