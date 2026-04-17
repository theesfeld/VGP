/* VGP Launcher -- SMS (Stores Management System) MFD page.
 * App list as selectable stations; search box is the bore-sight input. */

#include "launcher.h"
#include "vgp-hud.h"

#include <stdio.h>

void launcher_render(launcher_t *l)
{
    vgfx_ctx_t *ctx = &l->ctx;
    hud_palette_t P = hud_palette();

    vgfx_clear(ctx, vgfx_rgba(0, 0, 0, 0));

    /* MFD frame with no OSBs (single-page selector).
     * Title: static ETCHED, result count: projected dynamic. */
    char title[64];
    snprintf(title, sizeof(title), "SMS-LAUNCHER   %d HITS", l->filtered_count);

    hud_mfd_t mfd = { 0 };
    mfd.title = title;
    float cx, cy, cw, ch;
    hud_mfd_frame(ctx, &mfd, &P, &cx, &cy, &cw, &ch);

    /* --- Search input: boxed text field (bore-sight) --- */
    float ih = 28.0f;
    float fs = 14.0f;
    vgfx_rect_outline(ctx, cx, cy, cw, ih, 1.0f, P.warn);
    /* Small etched prefix */
    hud_etched(ctx, "QRY", cx + 8, cy + ih * 0.5f + fs * 0.35f, fs - 2, &P);
    /* Input text (projected) */
    float qx = cx + 44.0f;
    if (l->input_len > 0) {
        vgfx_text_bold(ctx, l->input_buf, qx,
                        cy + ih * 0.5f + fs * 0.35f, fs, P.hi);
        float tw = vgfx_text_width(ctx, l->input_buf, l->input_len, fs);
        /* Caret */
        vgfx_rect(ctx, qx + tw + 2, cy + 5, 2, ih - 10, P.warn);
    } else {
        hud_etched(ctx, "type to search", qx,
                    cy + ih * 0.5f + fs * 0.35f, fs - 2, &P);
        vgfx_rect(ctx, qx, cy + 5, 2, ih - 10, P.warn);
    }

    /* --- Stations (app list) --- */
    float ly = cy + ih + 8.0f;
    float lh = ch - ih - 8.0f;
    float item_h = 22.0f;
    int visible = (int)(lh / item_h);
    if (visible < 1) visible = 1;

    /* Keep selection in view */
    if (l->selected_index < l->scroll_offset)
        l->scroll_offset = l->selected_index;
    if (l->selected_index >= l->scroll_offset + visible)
        l->scroll_offset = l->selected_index - visible + 1;

    vgfx_push_clip(ctx, cx, ly, cw, lh);

    for (int i = l->scroll_offset;
         i < l->filtered_count && (i - l->scroll_offset) < visible; i++) {
        float ry = ly + (float)(i - l->scroll_offset) * item_h;
        int app_idx = l->filtered[i].app_index;
        launcher_app_t *app = &l->app_list.apps[app_idx];
        bool sel = (i == l->selected_index);
        bool hover = (ctx->mouse_y >= ry && ctx->mouse_y < ry + item_h &&
                       ctx->mouse_x >= cx && ctx->mouse_x < cx + cw);

        /* Station number -- etched left column */
        char num[8];
        snprintf(num, sizeof(num), "%02d", i + 1);
        hud_etched(ctx, num, cx + 4,
                    ry + item_h * 0.5f + fs * 0.35f, fs - 2, &P);

        /* Name (projected) */
        vgfx_color_t nc = sel ? P.hi : (hover ? P.fg : P.fg);
        vgfx_text(ctx, app->name, cx + 40,
                   ry + item_h * 0.5f + fs * 0.35f, fs - 1, nc);

        /* Selection reticule */
        if (sel)
            hud_target_box(ctx, cx + 1, ry + 1, cw - 2, item_h - 2, P.warn);

        if (hover && ctx->mouse_clicked) {
            l->selected_index = i;
            ctx->dirty = true;
        }
    }

    vgfx_pop_clip(ctx);

    /* Scrollbar on right edge */
    if (l->filtered_count > visible)
        vgfx_scrollbar(ctx, cx + cw - 6, ly, lh,
                        visible, l->filtered_count, &l->scroll_offset);
}
