#ifndef VGP_TYPES_H
#define VGP_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* ============================================================
 * Geometry types
 * ============================================================ */

typedef struct vgp_point {
    int32_t x, y;
} vgp_point_t;

typedef struct vgp_pointf {
    float x, y;
} vgp_pointf_t;

typedef struct vgp_rect {
    int32_t x, y, w, h;
} vgp_rect_t;

typedef struct vgp_rectf {
    float x, y, w, h;
} vgp_rectf_t;

/* ============================================================
 * Color
 * ============================================================ */

typedef struct vgp_color {
    float r, g, b, a;
} vgp_color_t;

static inline vgp_color_t vgp_color_rgb(float r, float g, float b)
{
    return (vgp_color_t){ r, g, b, 1.0f };
}

static inline vgp_color_t vgp_color_rgba(float r, float g, float b, float a)
{
    return (vgp_color_t){ r, g, b, a };
}

static inline vgp_color_t vgp_color_hex(uint32_t hex)
{
    return (vgp_color_t){
        .r = ((hex >> 16) & 0xFF) / 255.0f,
        .g = ((hex >> 8) & 0xFF) / 255.0f,
        .b = (hex & 0xFF) / 255.0f,
        .a = 1.0f,
    };
}

/* ============================================================
 * Rect utilities
 * ============================================================ */

static inline bool vgp_rect_intersects(const vgp_rect_t *a, const vgp_rect_t *b)
{
    return a->x < b->x + b->w && a->x + a->w > b->x &&
           a->y < b->y + b->h && a->y + a->h > b->y;
}

static inline vgp_rect_t vgp_rect_union(const vgp_rect_t *a, const vgp_rect_t *b)
{
    int32_t x1 = a->x < b->x ? a->x : b->x;
    int32_t y1 = a->y < b->y ? a->y : b->y;
    int32_t x2 = (a->x + a->w) > (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int32_t y2 = (a->y + a->h) > (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    return (vgp_rect_t){ x1, y1, x2 - x1, y2 - y1 };
}

static inline bool vgp_rect_contains_point(const vgp_rect_t *r, int32_t x, int32_t y)
{
    return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static inline bool vgp_rect_is_empty(const vgp_rect_t *r)
{
    return r->w <= 0 || r->h <= 0;
}

/* ============================================================
 * Common constants
 * ============================================================ */

#define VGP_MAX_OUTPUTS    8
#define VGP_MAX_WINDOWS    256
#define VGP_MAX_TITLE_LEN  256

#endif /* VGP_TYPES_H */
