#ifndef VGP_HUD_H
#define VGP_HUD_H

/* VGP HUD helper -- shared F-16 HUD/MFD rendering primitives.
 * Header-only. Include after vgp-gfx.h.
 *
 * Design rules:
 *   - Static labels / decoration = ETCHED (dark shadow offset below-right).
 *   - Dynamic values = PROJECTED (bright, crisp).
 *   - Colors: white primary, red critical, yellow warning/accent, dim gray.
 *   - Every element is data; no decoration for its own sake.
 */

#include "vgp-gfx.h"
#include <stdio.h>

/* ------------------------------------------------------------
 * Palette
 *
 * Rule (per project direction):
 *   - STATIC (labels, frames, non-changing decoration) = BLACK
 *   - DYNAMIC (values, pointers, live state) = white / yellow / red
 *     phosphor
 * Black is the "etched into the glass" tone; the white/red/yellow
 * are "projected onto the glass" phosphor cues.
 * ------------------------------------------------------------ */

typedef struct {
    /* Dynamic phosphor colors */
    vgfx_color_t fg;    /* white phosphor -- primary dynamic readout */
    vgfx_color_t hi;    /* brightest white -- emphasis */
    vgfx_color_t warn;  /* yellow phosphor -- warning / accent */
    vgfx_color_t crit;  /* red phosphor -- critical / error */
    /* Static etch tones */
    vgfx_color_t etch;  /* near-black -- static labels, frames, decoration */
    vgfx_color_t dim;   /* legacy alias for etch */
    /* Infrastructure */
    vgfx_color_t bg;    /* transparent -- glass shows through */
    vgfx_color_t shade; /* thin dark fill behind etched text for readability */
} hud_palette_t;

static inline hud_palette_t hud_palette(void)
{
    hud_palette_t p;
    p.fg    = vgfx_rgba(0.95f, 0.95f, 0.95f, 1.0f);
    p.hi    = vgfx_rgba(1.0f,  1.0f,  1.0f,  1.0f);
    p.warn  = vgfx_rgba(1.0f,  0.85f, 0.0f,  1.0f);
    p.crit  = vgfx_rgba(1.0f,  0.30f, 0.30f, 1.0f);
    p.etch  = vgfx_rgba(0.0f,  0.0f,  0.0f,  0.85f);  /* static = black */
    p.dim   = p.etch;
    p.bg    = vgfx_rgba(0.0f,  0.0f,  0.0f,  0.0f);
    p.shade = vgfx_rgba(0.0f,  0.0f,  0.0f,  0.20f);
    return p;
}

static inline vgfx_color_t hud_usage_color(const hud_palette_t *p, float v)
{
    if (v >= 0.90f) return p->crit;
    if (v >= 0.75f) return p->warn;
    return p->fg;
}

/* ------------------------------------------------------------
 * Text: etched vs projected
 *
 * Etched: static decoration stamped into the glass -- dark shadow
 * offset below-right, main glyphs in dim gray.
 * Projected: dynamic data painted bright on top of the glass.
 * ------------------------------------------------------------ */

static inline void hud_etched(vgfx_ctx_t *ctx, const char *s,
                                float x, float y, float fs,
                                const hud_palette_t *p)
{
    /* Static text is black etching. No shadow needed -- black on a
     * bright sky already reads. Tight 0.5px outer edge gives it bite. */
    vgfx_text(ctx, s, x + 0.5f, y, fs, vgfx_rgba(1, 1, 1, 0.18f));
    vgfx_text(ctx, s, x,        y, fs, p->etch);
}

static inline void hud_etched_bold(vgfx_ctx_t *ctx, const char *s,
                                     float x, float y, float fs,
                                     const hud_palette_t *p)
{
    vgfx_text_bold(ctx, s, x + 0.5f, y, fs, vgfx_rgba(1, 1, 1, 0.18f));
    vgfx_text_bold(ctx, s, x,        y, fs, p->etch);
}

static inline void hud_projected(vgfx_ctx_t *ctx, const char *s,
                                   float x, float y, float fs,
                                   vgfx_color_t c)
{
    vgfx_text_bold(ctx, s, x, y, fs, c);
}

/* ------------------------------------------------------------
 * Boxed data field: [LABEL  VALUE]
 * Thin etched box, etched label on the left, projected value on right.
 * Returns x advance used. ------------------------------------------------------------ */

static inline float hud_boxed_field(vgfx_ctx_t *ctx,
                                      float x, float y, float w,
                                      float fs,
                                      const char *label, const char *value,
                                      vgfx_color_t val_color,
                                      const hud_palette_t *p)
{
    float h = fs + 8.0f;
    vgfx_rect_outline(ctx, x, y, w, h, 1.0f, p->dim);
    hud_etched(ctx, label, x + 6, y + h * 0.5f + fs * 0.35f, fs - 2, p);
    float vw = vgfx_text_width(ctx, value, -1, fs - 1);
    hud_projected(ctx, value, x + w - vw - 6,
                   y + h * 0.5f + fs * 0.35f, fs - 1, val_color);
    return h;
}

/* ------------------------------------------------------------
 * Targeting box: indicates the currently selected item. Four corner
 * brackets (like an F-16 TGP reticule) rather than a filled highlight.
 * ------------------------------------------------------------ */

static inline void hud_target_box(vgfx_ctx_t *ctx,
                                    float x, float y, float w, float h,
                                    vgfx_color_t c)
{
    float L = 10.0f;
    if (L > w * 0.3f) L = w * 0.3f;
    if (L > h * 0.3f) L = h * 0.3f;
    float t = 1.4f;
    /* TL */
    vgfx_line(ctx, x,         y, x + L, y, t, c);
    vgfx_line(ctx, x,         y, x,     y + L, t, c);
    /* TR */
    vgfx_line(ctx, x + w,     y, x + w - L, y, t, c);
    vgfx_line(ctx, x + w,     y, x + w,     y + L, t, c);
    /* BL */
    vgfx_line(ctx, x,         y + h, x + L, y + h, t, c);
    vgfx_line(ctx, x,         y + h, x,     y + h - L, t, c);
    /* BR */
    vgfx_line(ctx, x + w,     y + h, x + w - L, y + h, t, c);
    vgfx_line(ctx, x + w,     y + h, x + w,     y + h - L, t, c);
}

/* ------------------------------------------------------------
 * OSB (Option Select Button): a short boxed label on the MFD bezel.
 * 'active' draws a filled highlight, 'enabled' draws dim text.
 * ------------------------------------------------------------ */

typedef struct {
    const char *label;
    bool        active;
    bool        enabled;
} hud_osb_t;

static inline bool hud_osb_draw(vgfx_ctx_t *ctx,
                                  float x, float y, float w, float h,
                                  const hud_osb_t *osb,
                                  const hud_palette_t *p)
{
    bool hover = (ctx->mouse_x >= x && ctx->mouse_x < x + w &&
                   ctx->mouse_y >= y && ctx->mouse_y < y + h);
    vgfx_color_t tc = osb->active ? p->warn :
                       (osb->enabled ? p->fg : p->dim);
    if (hover && osb->enabled)
        vgfx_rect(ctx, x, y, w, h, vgfx_alpha(p->warn, 0.12f));
    vgfx_rect_outline(ctx, x, y, w, h, 1.0f,
                       osb->active ? p->warn : p->dim);
    float fs = h * 0.50f;
    if (fs > 12.0f) fs = 12.0f;
    float tw = vgfx_text_width(ctx, osb->label, -1, fs);
    if (tw > w - 4.0f) fs = fs * ((w - 4.0f) / tw);
    tw = vgfx_text_width(ctx, osb->label, -1, fs);
    vgfx_text_bold(ctx, osb->label, x + (w - tw) * 0.5f,
                    y + h * 0.5f + fs * 0.35f, fs, tc);
    return hover && osb->enabled && ctx->mouse_clicked;
}

/* ------------------------------------------------------------
 * MFD frame: draws the four OSB rows around a central content area.
 * Returns the inner content rect via out_cx/out_cy/out_cw/out_ch.
 * Each OSB row may be NULL (no buttons on that side).
 * ------------------------------------------------------------ */

typedef struct {
    /* OSB labels per edge -- NULL entry means "no button at that slot" */
    const hud_osb_t *top;     int top_count;
    const hud_osb_t *right;   int right_count;
    const hud_osb_t *bottom;  int bottom_count;
    const hud_osb_t *left;    int left_count;

    /* Optional header title (ETCHED, shown at the top inside the frame) */
    const char *title;

    /* Set by the caller; filled by hud_mfd_frame with click hit info */
    int clicked_edge;    /* 0=none, 1=top, 2=right, 3=bottom, 4=left */
    int clicked_index;   /* -1 if no click */
} hud_mfd_t;

static inline void hud_mfd_frame(vgfx_ctx_t *ctx,
                                   hud_mfd_t *mfd,
                                   const hud_palette_t *p,
                                   float *out_cx, float *out_cy,
                                   float *out_cw, float *out_ch)
{
    float w = ctx->width, h = ctx->height;
    float osb_h = 24.0f;
    float osb_side_w = 48.0f;
    float pad = 6.0f;

    mfd->clicked_edge = 0;
    mfd->clicked_index = -1;

    float top_h    = mfd->top_count    > 0 ? osb_h : 0.0f;
    float bot_h    = mfd->bottom_count > 0 ? osb_h : 0.0f;
    float left_w   = mfd->left_count   > 0 ? osb_side_w : 0.0f;
    float right_w  = mfd->right_count  > 0 ? osb_side_w : 0.0f;

    /* Top row */
    if (mfd->top_count > 0) {
        float avail = w - left_w - right_w - pad * 2;
        float bw = avail / (float)mfd->top_count - 2.0f;
        for (int i = 0; i < mfd->top_count; i++) {
            float bx = left_w + pad + (float)i * (bw + 2.0f);
            if (hud_osb_draw(ctx, bx, pad, bw, osb_h - pad,
                              &mfd->top[i], p)) {
                mfd->clicked_edge = 1; mfd->clicked_index = i;
            }
        }
    }
    /* Bottom row */
    if (mfd->bottom_count > 0) {
        float avail = w - left_w - right_w - pad * 2;
        float bw = avail / (float)mfd->bottom_count - 2.0f;
        for (int i = 0; i < mfd->bottom_count; i++) {
            float bx = left_w + pad + (float)i * (bw + 2.0f);
            if (hud_osb_draw(ctx, bx, h - osb_h + 2,
                              bw, osb_h - pad,
                              &mfd->bottom[i], p)) {
                mfd->clicked_edge = 3; mfd->clicked_index = i;
            }
        }
    }
    /* Left column */
    if (mfd->left_count > 0) {
        float avail = h - top_h - bot_h - pad * 2;
        float bh = avail / (float)mfd->left_count - 2.0f;
        if (bh > 40.0f) bh = 40.0f;
        for (int i = 0; i < mfd->left_count; i++) {
            float by = top_h + pad + (float)i * (bh + 2.0f);
            if (hud_osb_draw(ctx, pad, by, osb_side_w - pad, bh,
                              &mfd->left[i], p)) {
                mfd->clicked_edge = 4; mfd->clicked_index = i;
            }
        }
    }
    /* Right column */
    if (mfd->right_count > 0) {
        float avail = h - top_h - bot_h - pad * 2;
        float bh = avail / (float)mfd->right_count - 2.0f;
        if (bh > 40.0f) bh = 40.0f;
        for (int i = 0; i < mfd->right_count; i++) {
            float by = top_h + pad + (float)i * (bh + 2.0f);
            if (hud_osb_draw(ctx, w - osb_side_w + 2, by,
                              osb_side_w - pad, bh,
                              &mfd->right[i], p)) {
                mfd->clicked_edge = 2; mfd->clicked_index = i;
            }
        }
    }

    /* Content rect */
    float cx = left_w + pad;
    float cy = top_h + pad;
    float cw = w - left_w - right_w - pad * 2;
    float ch = h - top_h - bot_h - pad * 2;

    /* Thin inner frame around content */
    vgfx_rect_outline(ctx, cx, cy, cw, ch, 1.0f, p->dim);

    if (mfd->title) {
        /* Dark header strip with ETCHED title */
        float hdr_h = 22.0f;
        vgfx_rect(ctx, cx, cy, cw, hdr_h, p->shade);
        vgfx_line(ctx, cx, cy + hdr_h, cx + cw, cy + hdr_h, 1.0f, p->dim);
        hud_etched_bold(ctx, mfd->title, cx + 10,
                         cy + hdr_h * 0.5f + 12 * 0.35f, 12, p);
        cy += hdr_h + 4.0f;
        ch -= hdr_h + 4.0f;
    }

    *out_cx = cx + 6.0f;
    *out_cy = cy + 4.0f;
    *out_cw = cw - 12.0f;
    *out_ch = ch - 8.0f;
}

/* ------------------------------------------------------------
 * Altitude tape: vertical scale with numeric marks. Used for line
 * numbers in the editor, scroll position, etc.
 * ------------------------------------------------------------ */

static inline void hud_altitude_tape(vgfx_ctx_t *ctx,
                                       float x, float y, float w, float h,
                                       int first_value, int step,
                                       float row_h, int highlight_idx,
                                       const hud_palette_t *p)
{
    int rows = (int)(h / row_h);
    vgfx_line(ctx, x + w - 1, y, x + w - 1, y + h, 1.0f, p->dim);
    for (int i = 0; i < rows; i++) {
        float ry = y + (float)i * row_h;
        int val = first_value + i * step;
        char buf[16]; snprintf(buf, sizeof(buf), "%4d", val);
        float fs = row_h * 0.7f;
        if (fs > 12.0f) fs = 12.0f;
        vgfx_color_t c = (i == highlight_idx) ? p->warn : p->dim;
        float tw = vgfx_text_width(ctx, buf, -1, fs);
        vgfx_text(ctx, buf, x + w - 6 - tw,
                   ry + row_h * 0.5f + fs * 0.35f, fs, c);
        /* Tick */
        vgfx_line(ctx, x + w - 3, ry + row_h * 0.5f,
                         x + w,     ry + row_h * 0.5f, 0.5f, p->dim);
    }
}

#endif /* VGP_HUD_H */
