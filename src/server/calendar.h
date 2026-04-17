/* SPDX-License-Identifier: MIT */
#ifndef VGP_CALENDAR_H
#define VGP_CALENDAR_H

#include "render_backend.h"
#include <stdbool.h>
#include <time.h>

typedef struct vgp_calendar {
    bool visible;
    int  year, month;
    float x, y;     /* position (top-right aligned) */
} vgp_calendar_t;

void vgp_calendar_init(vgp_calendar_t *cal);
void vgp_calendar_toggle(vgp_calendar_t *cal, float x, float y);
void vgp_calendar_render(vgp_calendar_t *cal, vgp_render_backend_t *b, void *ctx,
                          float font_size);

#endif /* VGP_CALENDAR_H */