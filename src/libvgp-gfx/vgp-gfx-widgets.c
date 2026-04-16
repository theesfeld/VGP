/* VGP Graphical UI Toolkit -- Widget implementations
 * Immediate-mode widgets: call every frame, return interaction state. */

#include "vgp-gfx.h"
#include <stdio.h>
#include <string.h>

/* ============================================================
 * Internal helpers
 * ============================================================ */

static bool mouse_in(vgfx_ctx_t *ctx, float x, float y, float w, float h)
{
    return ctx->mouse_x >= x && ctx->mouse_x < x + w &&
           ctx->mouse_y >= y && ctx->mouse_y < y + h;
}

static uint32_t next_wid(vgfx_ctx_t *ctx)
{
    return ctx->next_id++;
}

#define T(slot) vgfx_theme_color(ctx, slot)
#define TF(field) (ctx->theme.field)

/* ============================================================
 * Text widgets
 * ============================================================ */

void vgfx_label(vgfx_ctx_t *ctx, float x, float y, const char *text)
{
    vgfx_text(ctx, text, x, y + TF(font_size), TF(font_size), T(VGP_THEME_FG_SECONDARY));
}

void vgfx_label_colored(vgfx_ctx_t *ctx, float x, float y, const char *text, vgfx_color_t c)
{
    vgfx_text(ctx, text, x, y + TF(font_size), TF(font_size), c);
}

void vgfx_heading(vgfx_ctx_t *ctx, float x, float y, const char *text)
{
    vgfx_text_bold(ctx, text, x, y + TF(font_size_large), TF(font_size_large), T(VGP_THEME_FG));
}

void vgfx_section(vgfx_ctx_t *ctx, float x, float y, float w, const char *title)
{
    float tw = vgfx_text_width(ctx, title, -1, TF(font_size));
    vgfx_text_bold(ctx, title, x, y + TF(font_size), TF(font_size), T(VGP_THEME_ACCENT));
    vgfx_line(ctx, x + tw + 8, y + TF(font_size) * 0.5f, x + w, y + TF(font_size) * 0.5f,
               1, T(VGP_THEME_BORDER));
}

void vgfx_separator(vgfx_ctx_t *ctx, float x, float y, float w)
{
    vgfx_line(ctx, x, y, x + w, y, 1, T(VGP_THEME_BORDER));
}

/* ============================================================
 * Button
 * ============================================================ */

bool vgfx_button(vgfx_ctx_t *ctx, float x, float y, float w, float h, const char *label)
{
    uint32_t id = next_wid(ctx);
    bool hover = mouse_in(ctx, x, y, w, h);
    bool clicked = hover && ctx->mouse_clicked;
    if (hover) ctx->hot_id = id;

    vgfx_color_t bg = hover ? T(VGP_THEME_ACCENT_HOVER) : T(VGP_THEME_ACCENT);
    if (hover && ctx->mouse_pressed) bg = vgfx_alpha(bg, 0.8f);

    vgfx_rounded_rect(ctx, x, y, w, h, TF(corner_radius) * 0.5f, bg);

    float tw = vgfx_text_width(ctx, label, -1, TF(font_size));
    float tx = x + (w - tw) * 0.5f;
    float ty = y + (h + TF(font_size)) * 0.5f - 2;
    vgfx_text_bold(ctx, label, tx, ty, TF(font_size), vgfx_rgb(1, 1, 1));

    return clicked;
}

/* ============================================================
 * Checkbox
 * ============================================================ */

bool vgfx_checkbox(vgfx_ctx_t *ctx, float x, float y, const char *label, bool *value)
{
    uint32_t id = next_wid(ctx);
    float sz = TF(checkbox_size);
    float total_w = sz + 8 + vgfx_text_width(ctx, label, -1, TF(font_size));
    bool hover = mouse_in(ctx, x, y, total_w, sz);
    bool clicked = hover && ctx->mouse_clicked;
    if (hover) ctx->hot_id = id;

    /* Box */
    vgfx_color_t box_bg = *value ? T(VGP_THEME_ACCENT) :
                            (hover ? T(VGP_THEME_BG_TERTIARY) : T(VGP_THEME_BG_SECONDARY));
    vgfx_rounded_rect(ctx, x, y, sz, sz, 3, box_bg);
    vgfx_rounded_rect_outline(ctx, x, y, sz, sz, 3, 1,
                                hover ? T(VGP_THEME_ACCENT) : T(VGP_THEME_BORDER));

    /* Checkmark */
    if (*value) {
        float cx = x + sz * 0.5f, cy = y + sz * 0.5f;
        float s = sz * 0.3f;
        vgfx_line(ctx, cx - s, cy, cx - s * 0.3f, cy + s * 0.7f, 2, vgfx_rgb(1, 1, 1));
        vgfx_line(ctx, cx - s * 0.3f, cy + s * 0.7f, cx + s, cy - s * 0.5f, 2, vgfx_rgb(1, 1, 1));
    }

    /* Label */
    vgfx_text(ctx, label, x + sz + 8, y + sz * 0.5f + TF(font_size) * 0.35f,
               TF(font_size), hover ? T(VGP_THEME_FG) : T(VGP_THEME_FG_SECONDARY));

    if (clicked) { *value = !*value; return true; }
    return false;
}

/* ============================================================
 * Slider
 * ============================================================ */

bool vgfx_slider(vgfx_ctx_t *ctx, float x, float y, float w,
                   float *value, float min, float max, const char *fmt)
{
    uint32_t id = next_wid(ctx);
    float h = TF(slider_height);
    float thumb_r = h * 1.5f;
    float track_y = y + thumb_r - h * 0.5f;
    float range = max - min;
    if (range <= 0) range = 1;
    float norm = (*value - min) / range;
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;

    float track_w = w - 60; /* leave room for value text */
    bool hover = mouse_in(ctx, x, track_y - thumb_r, track_w, thumb_r * 2 + h);
    if (hover) ctx->hot_id = id;

    /* Track background */
    vgfx_rounded_rect(ctx, x, track_y, track_w, h, h * 0.5f, T(VGP_THEME_BG_SECONDARY));
    /* Filled portion */
    float fill_w = norm * track_w;
    if (fill_w > h) vgfx_rounded_rect(ctx, x, track_y, fill_w, h, h * 0.5f, T(VGP_THEME_ACCENT));
    /* Thumb */
    float thumb_x = x + fill_w;
    vgfx_circle(ctx, thumb_x, track_y + h * 0.5f, thumb_r,
                 hover ? T(VGP_THEME_ACCENT_HOVER) : T(VGP_THEME_ACCENT));

    /* Value text */
    char vbuf[32];
    snprintf(vbuf, sizeof(vbuf), fmt ? fmt : "%.1f", *value);
    vgfx_text(ctx, vbuf, x + track_w + 8, y + thumb_r + TF(font_size) * 0.35f,
               TF(font_size), T(VGP_THEME_FG));

    /* Interaction */
    bool changed = false;
    if (hover && ctx->mouse_clicked) {
        ctx->active_id = id;
    }
    if (ctx->active_id == id && ctx->mouse_pressed) {
        float click_x = ctx->mouse_x - x;
        float new_norm = click_x / track_w;
        if (new_norm < 0) new_norm = 0;
        if (new_norm > 1) new_norm = 1;
        float new_val = min + new_norm * range;
        if (new_val != *value) { *value = new_val; changed = true; }
    }
    if (!ctx->mouse_pressed && ctx->active_id == id)
        ctx->active_id = 0;

    return changed;
}

/* ============================================================
 * Dropdown
 * ============================================================ */

bool vgfx_dropdown(vgfx_ctx_t *ctx, float x, float y, float w,
                     const char **items, int count, int *selected, bool *open)
{
    uint32_t id = next_wid(ctx);
    float h = TF(input_height);
    bool changed = false;
    bool hover_main = mouse_in(ctx, x, y, w, h);
    if (hover_main) ctx->hot_id = id;

    /* Main box */
    vgfx_rounded_rect(ctx, x, y, w, h, 4,
                        hover_main ? T(VGP_THEME_BG_TERTIARY) : T(VGP_THEME_BG_SECONDARY));
    vgfx_rounded_rect_outline(ctx, x, y, w, h, 4, 1,
                                *open ? T(VGP_THEME_ACCENT) : T(VGP_THEME_BORDER));

    /* Current selection text */
    const char *cur = (*selected >= 0 && *selected < count) ? items[*selected] : "---";
    vgfx_text(ctx, cur, x + TF(padding), y + h * 0.5f + TF(font_size) * 0.35f,
               TF(font_size), T(VGP_THEME_FG));

    /* Down arrow */
    float ax = x + w - 16, ay = y + h * 0.5f;
    vgfx_line(ctx, ax - 4, ay - 2, ax, ay + 2, 1.5f, T(VGP_THEME_FG_SECONDARY));
    vgfx_line(ctx, ax, ay + 2, ax + 4, ay - 2, 1.5f, T(VGP_THEME_FG_SECONDARY));

    if (hover_main && ctx->mouse_clicked) *open = !*open;

    /* Dropdown list */
    if (*open) {
        float ly = y + h + 2;
        float lh = (float)count * h;
        /* Background */
        vgfx_rounded_rect(ctx, x, ly, w, lh, 4, T(VGP_THEME_BG_SECONDARY));
        vgfx_rounded_rect_outline(ctx, x, ly, w, lh, 4, 1, T(VGP_THEME_BORDER));

        for (int i = 0; i < count; i++) {
            float iy = ly + (float)i * h;
            bool ih = mouse_in(ctx, x, iy, w, h);
            if (ih)
                vgfx_rounded_rect(ctx, x + 2, iy + 1, w - 4, h - 2, 3, T(VGP_THEME_ACCENT));
            vgfx_text(ctx, items[i], x + TF(padding), iy + h * 0.5f + TF(font_size) * 0.35f,
                        TF(font_size), ih ? vgfx_rgb(1, 1, 1) : T(VGP_THEME_FG));
            if (ih && ctx->mouse_clicked) {
                *selected = i;
                *open = false;
                changed = true;
            }
        }

        /* Click outside closes */
        if (ctx->mouse_clicked && !hover_main && !mouse_in(ctx, x, ly, w, lh))
            *open = false;
    }

    return changed;
}

/* ============================================================
 * Text input
 * ============================================================ */

bool vgfx_text_input(vgfx_ctx_t *ctx, float x, float y, float w,
                       char *buffer, int buf_size)
{
    uint32_t id = next_wid(ctx);
    float h = TF(input_height);
    bool hover = mouse_in(ctx, x, y, w, h);
    bool focused = (ctx->focus_id == id);

    /* Background */
    vgfx_rounded_rect(ctx, x, y, w, h, 4,
                        focused ? T(VGP_THEME_BG_TERTIARY) : T(VGP_THEME_BG_SECONDARY));
    vgfx_rounded_rect_outline(ctx, x, y, w, h, 4, 1,
                                focused ? T(VGP_THEME_ACCENT) : T(VGP_THEME_BORDER));

    /* Text */
    float tx = x + TF(padding);
    float ty = y + h * 0.5f + TF(font_size) * 0.35f;
    vgfx_text(ctx, buffer, tx, ty, TF(font_size),
               focused ? T(VGP_THEME_FG) : T(VGP_THEME_FG_SECONDARY));

    /* Cursor */
    if (focused) {
        float cw = vgfx_text_width(ctx, buffer, -1, TF(font_size));
        vgfx_rect(ctx, tx + cw + 1, y + 4, 2, h - 8, T(VGP_THEME_ACCENT));
    }

    /* Click to focus */
    if (hover && ctx->mouse_clicked)
        ctx->focus_id = id;

    /* Typing */
    if (focused && ctx->key_pressed) {
        int len = (int)strlen(buffer);
        if (ctx->last_keysym == 0xFF08 && len > 0) {
            buffer[len - 1] = '\0';
        } else if (ctx->last_keysym == 0xFF0D || ctx->last_keysym == 0xFF09) {
            ctx->focus_id = 0;
            return true;
        } else if (ctx->last_keysym == 0xFF1B) {
            ctx->focus_id = 0;
        } else if (ctx->last_utf8[0] >= 0x20 && len < buf_size - 1) {
            buffer[len] = ctx->last_utf8[0];
            buffer[len + 1] = '\0';
        }
    }
    return false;
}

/* ============================================================
 * List item
 * ============================================================ */

bool vgfx_list_item(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                      const char *text, bool selected)
{
    uint32_t id = next_wid(ctx);
    bool hover = mouse_in(ctx, x, y, w, h);
    if (hover) ctx->hot_id = id;

    vgfx_color_t bg = selected ? T(VGP_THEME_ACCENT) :
                        (hover ? T(VGP_THEME_BG_TERTIARY) : T(VGP_THEME_BG));
    vgfx_rounded_rect(ctx, x, y, w, h, 4, bg);

    if (selected)
        vgfx_rect(ctx, x, y + 2, 3, h - 4, vgfx_rgb(1, 1, 1));

    vgfx_text(ctx, text, x + TF(padding) + (selected ? 6 : 0),
               y + h * 0.5f + TF(font_size) * 0.35f, TF(font_size),
               selected ? vgfx_rgb(1, 1, 1) : (hover ? T(VGP_THEME_FG) : T(VGP_THEME_FG_SECONDARY)));

    return hover && ctx->mouse_clicked;
}

/* ============================================================
 * Progress bar
 * ============================================================ */

void vgfx_progress(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                     float value, vgfx_color_t c)
{
    vgfx_rounded_rect(ctx, x, y, w, h, h * 0.5f, T(VGP_THEME_BG_SECONDARY));
    float fw = value * w;
    if (fw > h)
        vgfx_rounded_rect(ctx, x, y, fw, h, h * 0.5f, c);
}

/* ============================================================
 * Tooltip
 * ============================================================ */

void vgfx_tooltip(vgfx_ctx_t *ctx, float area_x, float area_y, float area_w, float area_h,
                    const char *text)
{
    if (!mouse_in(ctx, area_x, area_y, area_w, area_h)) return;

    float tw = vgfx_text_width(ctx, text, -1, TF(font_size_small)) + TF(padding) * 2;
    float th = TF(font_size_small) + TF(padding) * 2;
    float tx = ctx->mouse_x + 12;
    float ty = ctx->mouse_y + 16;
    if (tx + tw > ctx->width) tx = ctx->width - tw - 4;
    if (ty + th > ctx->height) ty = ctx->mouse_y - th - 4;

    vgfx_rounded_rect(ctx, tx, ty, tw, th, 4, T(VGP_THEME_TOOLTIP_BG));
    vgfx_rounded_rect_outline(ctx, tx, ty, tw, th, 4, 1, T(VGP_THEME_BORDER));
    vgfx_text(ctx, text, tx + TF(padding), ty + th * 0.5f + TF(font_size_small) * 0.35f,
               TF(font_size_small), T(VGP_THEME_FG));
}

/* ============================================================
 * Scrollbar
 * ============================================================ */

bool vgfx_scrollbar(vgfx_ctx_t *ctx, float x, float y, float h,
                      int visible, int total, int *offset)
{
    if (total <= visible) return false;
    float sw = TF(scrollbar_width);
    float ratio = (float)visible / (float)total;
    float thumb_h = ratio * h;
    if (thumb_h < 20) thumb_h = 20;
    float max_scroll = (float)(total - visible);
    float thumb_y = y + ((float)*offset / max_scroll) * (h - thumb_h);

    /* Track */
    vgfx_rounded_rect(ctx, x, y, sw, h, sw * 0.5f, T(VGP_THEME_SCROLLBAR));
    /* Thumb */
    bool hover = mouse_in(ctx, x, thumb_y, sw, thumb_h);
    vgfx_rounded_rect(ctx, x, thumb_y, sw, thumb_h, sw * 0.5f,
                        hover ? T(VGP_THEME_ACCENT) : T(VGP_THEME_SCROLLBAR_THUMB));

    /* Scroll wheel */
    if (ctx->scroll_dy != 0) {
        int new_off = *offset - (int)(ctx->scroll_dy / 15.0f);
        if (new_off < 0) new_off = 0;
        if (new_off > (int)max_scroll) new_off = (int)max_scroll;
        if (new_off != *offset) { *offset = new_off; return true; }
    }
    return false;
}
