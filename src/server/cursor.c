/* SPDX-License-Identifier: MIT */
#include "cursor.h"

#define CURSOR_SIZE 16

void vgp_cursor_init(vgp_cursor_t *cursor)
{
    cursor->x = 0;
    cursor->y = 0;
    cursor->prev_x = 0;
    cursor->prev_y = 0;
    cursor->visible = true;
    cursor->moved = false;
    cursor->buttons = 0;
}

void vgp_cursor_move(vgp_cursor_t *cursor, float dx, float dy,
                      uint32_t screen_w, uint32_t screen_h)
{
    cursor->prev_x = cursor->x;
    cursor->prev_y = cursor->y;

    cursor->x += dx;
    cursor->y += dy;

    /* Clamp to screen bounds */
    if (cursor->x < 0) cursor->x = 0;
    if (cursor->y < 0) cursor->y = 0;
    if (cursor->x >= (float)screen_w) cursor->x = (float)screen_w - 1;
    if (cursor->y >= (float)screen_h) cursor->y = (float)screen_h - 1;

    cursor->moved = true;
}

void vgp_cursor_set_position(vgp_cursor_t *cursor, float x, float y)
{
    cursor->prev_x = cursor->x;
    cursor->prev_y = cursor->y;
    cursor->x = x;
    cursor->y = y;
    cursor->moved = true;
}

void vgp_cursor_render(vgp_cursor_t *cursor, plutovg_canvas_t *canvas)
{
    if (!cursor->visible)
        return;

    float x = cursor->x;
    float y = cursor->y;

    /* Draw an arrow cursor as a vector path */
    plutovg_canvas_save(canvas);

    /* Arrow outline (black) */
    plutovg_canvas_move_to(canvas, x, y);
    plutovg_canvas_line_to(canvas, x, y + CURSOR_SIZE);
    plutovg_canvas_line_to(canvas, x + 4, y + CURSOR_SIZE - 4);
    plutovg_canvas_line_to(canvas, x + 7, y + CURSOR_SIZE + 2);
    plutovg_canvas_line_to(canvas, x + 9, y + CURSOR_SIZE);
    plutovg_canvas_line_to(canvas, x + 6, y + CURSOR_SIZE - 5);
    plutovg_canvas_line_to(canvas, x + CURSOR_SIZE - 4, y + CURSOR_SIZE - 5);
    plutovg_canvas_close_path(canvas);

    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_set_line_width(canvas, 1.5f);
    plutovg_canvas_stroke_preserve(canvas);

    /* Arrow fill (white) */
    plutovg_canvas_set_rgb(canvas, 1, 1, 1);
    plutovg_canvas_fill(canvas);

    plutovg_canvas_restore(canvas);
    cursor->moved = false;
}

void vgp_cursor_get_damage(vgp_cursor_t *cursor,
                            vgp_rect_t *old_rect, vgp_rect_t *new_rect)
{
    int pad = 4;
    *old_rect = (vgp_rect_t){
        (int32_t)cursor->prev_x - pad,
        (int32_t)cursor->prev_y - pad,
        CURSOR_SIZE + pad * 2 + 10,
        CURSOR_SIZE + pad * 2 + 10,
    };
    *new_rect = (vgp_rect_t){
        (int32_t)cursor->x - pad,
        (int32_t)cursor->y - pad,
        CURSOR_SIZE + pad * 2 + 10,
        CURSOR_SIZE + pad * 2 + 10,
    };
}