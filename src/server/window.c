#include "window.h"

vgp_rect_t vgp_window_content_rect(const vgp_rect_t *frame,
                                     const vgp_theme_t *theme)
{
    int32_t bw = (int32_t)theme->border_width;
    int32_t th = (int32_t)theme->titlebar_height;
    return (vgp_rect_t){
        .x = frame->x + bw,
        .y = frame->y + th,
        .w = frame->w - bw * 2,
        .h = frame->h - th - bw,
    };
}

vgp_rect_t vgp_window_frame_rect(int32_t cx, int32_t cy,
                                   uint32_t cw, uint32_t ch,
                                   const vgp_theme_t *theme)
{
    int32_t bw = (int32_t)theme->border_width;
    int32_t th = (int32_t)theme->titlebar_height;
    return (vgp_rect_t){
        .x = cx - bw,
        .y = cy - th,
        .w = (int32_t)cw + bw * 2,
        .h = (int32_t)ch + th + bw,
    };
}

vgp_hit_region_t vgp_window_hit_test(const vgp_window_t *win,
                                      const vgp_theme_t *theme,
                                      int32_t x, int32_t y)
{
    const vgp_rect_t *f = &win->frame_rect;
    int32_t bw = (int32_t)theme->border_width;
    int32_t th = (int32_t)theme->titlebar_height;
    int32_t grab_zone = bw < 5 ? 5 : bw; /* minimum 5px grab zone for borders */

    /* Check if point is outside the frame entirely */
    if (!vgp_rect_contains_point(f, x, y))
        return VGP_HIT_NONE;

    int32_t rel_x = x - f->x;
    int32_t rel_y = y - f->y;

    /* Check border regions (corner zones first, then edges) */
    bool at_top    = rel_y < grab_zone;
    bool at_bottom = rel_y >= f->h - grab_zone;
    bool at_left   = rel_x < grab_zone;
    bool at_right  = rel_x >= f->w - grab_zone;

    if (at_top && at_left)   return VGP_HIT_BORDER_NW;
    if (at_top && at_right)  return VGP_HIT_BORDER_NE;
    if (at_bottom && at_left)  return VGP_HIT_BORDER_SW;
    if (at_bottom && at_right) return VGP_HIT_BORDER_SE;
    if (at_top)    return VGP_HIT_BORDER_N;
    if (at_bottom) return VGP_HIT_BORDER_S;
    if (at_left)   return VGP_HIT_BORDER_W;
    if (at_right)  return VGP_HIT_BORDER_E;

    /* Check titlebar region */
    if (rel_y < th) {
        /* Check buttons (right-aligned in titlebar) */
        float btn_r = theme->button_radius;
        float btn_spacing = theme->button_spacing;
        float btn_margin = theme->button_margin_right;
        float btn_cy = th / 2.0f;

        /* Close button (rightmost) */
        float close_cx = (float)f->w - btn_margin - btn_r;
        float dx = (float)rel_x - close_cx;
        float dy = (float)rel_y - btn_cy;
        if (dx * dx + dy * dy <= (btn_r + 2) * (btn_r + 2))
            return VGP_HIT_CLOSE_BTN;

        /* Maximize button */
        float max_cx = close_cx - btn_r * 2 - btn_spacing;
        dx = (float)rel_x - max_cx;
        dy = (float)rel_y - btn_cy;
        if (dx * dx + dy * dy <= (btn_r + 2) * (btn_r + 2))
            return VGP_HIT_MAXIMIZE_BTN;

        /* Minimize button */
        float min_cx = max_cx - btn_r * 2 - btn_spacing;
        dx = (float)rel_x - min_cx;
        dy = (float)rel_y - btn_cy;
        if (dx * dx + dy * dy <= (btn_r + 2) * (btn_r + 2))
            return VGP_HIT_MINIMIZE_BTN;

        return VGP_HIT_TITLEBAR;
    }

    /* Content area */
    if (rel_x >= bw && rel_x < f->w - bw &&
        rel_y >= th && rel_y < f->h - bw)
        return VGP_HIT_CONTENT;

    return VGP_HIT_NONE;
}
