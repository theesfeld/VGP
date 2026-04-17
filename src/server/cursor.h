/* SPDX-License-Identifier: MIT */
#ifndef VGP_CURSOR_H
#define VGP_CURSOR_H

#include "vgp/types.h"
#include <stdbool.h>

typedef enum {
    VGP_CURSOR_ARROW,
    VGP_CURSOR_RESIZE_N,
    VGP_CURSOR_RESIZE_S,
    VGP_CURSOR_RESIZE_E,
    VGP_CURSOR_RESIZE_W,
    VGP_CURSOR_RESIZE_NE,
    VGP_CURSOR_RESIZE_NW,
    VGP_CURSOR_RESIZE_SE,
    VGP_CURSOR_RESIZE_SW,
    VGP_CURSOR_MOVE,
    VGP_CURSOR_TEXT,
    VGP_CURSOR_HAND,
} vgp_cursor_shape_t;

typedef struct vgp_cursor {
    float    x, y;
    float    prev_x, prev_y;
    bool     visible;
    bool     moved;
    uint32_t buttons;
    vgp_cursor_shape_t shape;
} vgp_cursor_t;

void vgp_cursor_init(vgp_cursor_t *cursor);
void vgp_cursor_move(vgp_cursor_t *cursor, float dx, float dy,
                      uint32_t screen_w, uint32_t screen_h);
void vgp_cursor_set_position(vgp_cursor_t *cursor, float x, float y);

/* Get damage rects for cursor movement (old + new positions) */
void vgp_cursor_get_damage(vgp_cursor_t *cursor,
                            vgp_rect_t *old_rect, vgp_rect_t *new_rect);

#endif /* VGP_CURSOR_H */