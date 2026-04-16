#include "launcher.h"
#include <stdio.h>
#include <string.h>

void launcher_render(launcher_t *l)
{
    vgfx_ctx_t *ctx = &l->ctx;
    float w = ctx->width, h = ctx->height;
    float p = ctx->theme.padding;
    float fs = ctx->theme.font_size;

    /* Background with slight transparency */
    vgfx_clear(ctx, vgfx_theme_color(ctx, VGP_THEME_BG));
    vgfx_rounded_rect_outline(ctx, 1, 1, w - 2, h - 2, 10, 2,
                                vgfx_theme_color(ctx, VGP_THEME_ACCENT));

    /* Title */
    vgfx_text_bold(ctx, "VGP Launcher", p + 4, p + fs + 4, fs + 2,
                     vgfx_theme_color(ctx, VGP_THEME_ACCENT));

    /* Input box */
    float iy = p * 2 + fs + 12;
    float ih = ctx->theme.input_height + 4;
    vgfx_rounded_rect(ctx, p, iy, w - p * 2, ih, 6,
                        vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));
    vgfx_rounded_rect_outline(ctx, p, iy, w - p * 2, ih, 6, 1,
                                vgfx_theme_color(ctx, VGP_THEME_ACCENT));

    if (l->input_len > 0) {
        vgfx_text(ctx, l->input_buf, p + 10, iy + ih * 0.5f + fs * 0.35f, fs,
                    vgfx_theme_color(ctx, VGP_THEME_FG));
        /* Cursor */
        float cw = vgfx_text_width(ctx, l->input_buf, l->input_len, fs);
        vgfx_rect(ctx, p + 10 + cw + 1, iy + 5, 2, ih - 10,
                    vgfx_theme_color(ctx, VGP_THEME_ACCENT));
    } else {
        vgfx_text(ctx, "Type to search...", p + 10, iy + ih * 0.5f + fs * 0.35f, fs,
                    vgfx_theme_color(ctx, VGP_THEME_FG_DISABLED));
        vgfx_rect(ctx, p + 10, iy + 5, 2, ih - 10, vgfx_theme_color(ctx, VGP_THEME_ACCENT));
    }

    /* Results list */
    float ly = iy + ih + p;
    float item_h = fs + 14;
    float list_h = h - ly - 30;
    int visible = (int)(list_h / item_h);
    if (visible < 1) visible = 1;

    /* Scroll to keep selection visible */
    if (l->selected_index < l->scroll_offset)
        l->scroll_offset = l->selected_index;
    if (l->selected_index >= l->scroll_offset + visible)
        l->scroll_offset = l->selected_index - visible + 1;

    vgfx_push_clip(ctx, p, ly, w - p * 2, list_h);
    for (int i = l->scroll_offset; i < l->filtered_count && (i - l->scroll_offset) < visible; i++) {
        float ry = ly + (float)(i - l->scroll_offset) * item_h;
        int app_idx = l->filtered[i].app_index;
        launcher_app_t *app = &l->app_list.apps[app_idx];
        bool sel = (i == l->selected_index);

        if (sel) {
            vgfx_rounded_rect(ctx, p + 2, ry, w - p * 2 - 4, item_h - 2, 6,
                                vgfx_alpha(vgfx_theme_color(ctx, VGP_THEME_ACCENT), 0.25f));
            /* Accent bar */
            vgfx_rounded_rect(ctx, p + 4, ry + 4, 3, item_h - 10, 1.5f,
                                vgfx_theme_color(ctx, VGP_THEME_ACCENT));
        }

        vgfx_color_t text_c = sel ? vgfx_theme_color(ctx, VGP_THEME_FG) :
                                      vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY);
        vgfx_text(ctx, app->name, p + 16, ry + item_h * 0.5f + fs * 0.35f, fs, text_c);

        /* Mouse click */
        if (ctx->mouse_clicked && ctx->mouse_y >= ry && ctx->mouse_y < ry + item_h &&
            ctx->mouse_x >= p && ctx->mouse_x < w - p) {
            l->selected_index = i;
            ctx->dirty = true;
        }
    }
    vgfx_pop_clip(ctx);

    /* Result count */
    char count[32];
    snprintf(count, sizeof(count), "%d results", l->filtered_count);
    vgfx_text(ctx, count, p, h - 18, fs - 2, vgfx_theme_color(ctx, VGP_THEME_FG_DISABLED));
}
