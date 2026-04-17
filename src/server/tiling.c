/* SPDX-License-Identifier: MIT */
#include "tiling.h"
#include <string.h>
#include <strings.h>

#define PHI 1.618033988f

vgp_tile_algorithm_t vgp_tile_parse_algorithm(const char *name)
{
    if (!name) return VGP_TILE_GOLDEN_RATIO;
    if (strcasecmp(name, "golden") == 0 || strcasecmp(name, "golden_ratio") == 0)
        return VGP_TILE_GOLDEN_RATIO;
    if (strcasecmp(name, "equal") == 0)
        return VGP_TILE_EQUAL;
    if (strcasecmp(name, "master") == 0 || strcasecmp(name, "master_stack") == 0)
        return VGP_TILE_MASTER_STACK;
    if (strcasecmp(name, "spiral") == 0 || strcasecmp(name, "fibonacci") == 0)
        return VGP_TILE_SPIRAL;
    return VGP_TILE_GOLDEN_RATIO;
}

const char *vgp_tile_algorithm_name(vgp_tile_algorithm_t algo)
{
    switch (algo) {
    case VGP_TILE_GOLDEN_RATIO: return "golden_ratio";
    case VGP_TILE_EQUAL:        return "equal";
    case VGP_TILE_MASTER_STACK: return "master_stack";
    case VGP_TILE_SPIRAL:       return "spiral";
    }
    return "golden_ratio";
}

static void apply_gaps(vgp_rect_t *r, int gap)
{
    r->x += gap;
    r->y += gap;
    r->w -= gap * 2;
    r->h -= gap * 2;
    if (r->w < 50) r->w = 50;
    if (r->h < 50) r->h = 50;
}

/* ============================================================
 * Golden Ratio: first window gets phi/(1+phi) of space,
 * remaining windows recursively split the rest.
 * ============================================================ */

static int tile_golden_ratio(vgp_rect_t area, int count, int gap,
                              vgp_split_dir_t dir, vgp_rect_t *out)
{
    if (count <= 0) return 0;

    if (count == 1) {
        out[0] = area;
        apply_gaps(&out[0], gap);
        return 1;
    }

    float ratio = PHI / (1.0f + PHI); /* ~0.618 */

    vgp_rect_t first, rest;
    if (dir == VGP_SPLIT_HORIZONTAL) {
        int split_w = (int)((float)area.w * ratio);
        first = (vgp_rect_t){ area.x, area.y, split_w, area.h };
        rest = (vgp_rect_t){ area.x + split_w, area.y, area.w - split_w, area.h };
    } else {
        int split_h = (int)((float)area.h * ratio);
        first = (vgp_rect_t){ area.x, area.y, area.w, split_h };
        rest = (vgp_rect_t){ area.x, area.y + split_h, area.w, area.h - split_h };
    }

    out[0] = first;
    apply_gaps(&out[0], gap);

    /* Recurse with alternating split direction */
    vgp_split_dir_t next_dir = (dir == VGP_SPLIT_HORIZONTAL) ?
        VGP_SPLIT_VERTICAL : VGP_SPLIT_HORIZONTAL;
    return 1 + tile_golden_ratio(rest, count - 1, gap, next_dir, out + 1);
}

/* ============================================================
 * Equal: all windows get equal space in a grid.
 * ============================================================ */

static int tile_equal(vgp_rect_t area, int count, int gap, vgp_rect_t *out)
{
    if (count <= 0) return 0;

    /* Calculate grid: cols x rows */
    int cols = 1;
    while (cols * cols < count) cols++;
    int rows = (count + cols - 1) / cols;

    int cell_w = area.w / cols;
    int cell_h = area.h / rows;

    for (int i = 0; i < count; i++) {
        int col = i % cols;
        int row = i / cols;
        out[i] = (vgp_rect_t){
            area.x + col * cell_w,
            area.y + row * cell_h,
            cell_w,
            cell_h,
        };
        apply_gaps(&out[i], gap);
    }
    return count;
}

/* ============================================================
 * Master + Stack: one large master on left, rest stacked on right.
 * ============================================================ */

static int tile_master_stack(vgp_rect_t area, int count, int gap,
                              float master_ratio, vgp_rect_t *out)
{
    if (count <= 0) return 0;

    if (count == 1) {
        out[0] = area;
        apply_gaps(&out[0], gap);
        return 1;
    }

    int master_w = (int)((float)area.w * master_ratio);
    int stack_w = area.w - master_w;
    int stack_count = count - 1;
    int stack_h = area.h / stack_count;

    /* Master */
    out[0] = (vgp_rect_t){ area.x, area.y, master_w, area.h };
    apply_gaps(&out[0], gap);

    /* Stack */
    for (int i = 0; i < stack_count; i++) {
        out[i + 1] = (vgp_rect_t){
            area.x + master_w,
            area.y + i * stack_h,
            stack_w,
            stack_h,
        };
        apply_gaps(&out[i + 1], gap);
    }
    return count;
}

/* ============================================================
 * Spiral (Fibonacci): like golden ratio but spiraling inward.
 * ============================================================ */

static int tile_spiral(vgp_rect_t area, int count, int gap, vgp_rect_t *out)
{
    if (count <= 0) return 0;

    vgp_rect_t remaining = area;

    for (int i = 0; i < count; i++) {
        if (i == count - 1) {
            /* Last window gets all remaining space */
            out[i] = remaining;
            apply_gaps(&out[i], gap);
            break;
        }

        /* Split: alternate between horizontal and vertical */
        int dir = i % 4; /* 0=right, 1=bottom, 2=left, 3=top */
        float ratio = 0.5f; /* equal split for spiral */

        switch (dir) {
        case 0: { /* split right */
            int w = (int)((float)remaining.w * ratio);
            out[i] = (vgp_rect_t){ remaining.x, remaining.y, w, remaining.h };
            remaining.x += w;
            remaining.w -= w;
            break;
        }
        case 1: { /* split bottom */
            int h = (int)((float)remaining.h * ratio);
            out[i] = (vgp_rect_t){ remaining.x, remaining.y, remaining.w, h };
            remaining.y += h;
            remaining.h -= h;
            break;
        }
        case 2: { /* split left (from right side) */
            int w = (int)((float)remaining.w * ratio);
            out[i] = (vgp_rect_t){ remaining.x + remaining.w - w, remaining.y, w, remaining.h };
            remaining.w -= w;
            break;
        }
        case 3: { /* split top (from bottom side) */
            int h = (int)((float)remaining.h * ratio);
            out[i] = (vgp_rect_t){ remaining.x, remaining.y + remaining.h - h, remaining.w, h };
            remaining.h -= h;
            break;
        }
        }
        apply_gaps(&out[i], gap);
    }
    return count;
}

/* ============================================================
 * Main entry point
 * ============================================================ */

int vgp_tile_calculate(vgp_tile_config_t *config, int window_count,
                        vgp_rect_t area, vgp_rect_t *out_rects)
{
    if (window_count <= 0 || window_count > VGP_TILE_MAX_WINDOWS)
        return 0;

    /* Apply outer gaps */
    int outer = config->gap_outer;
    area.x += outer;
    area.y += outer;
    area.w -= outer * 2;
    area.h -= outer * 2;

    int gap = config->gap_inner;

    /* Smart gaps: no gaps with single window */
    if (config->smart_gaps && window_count == 1)
        gap = 0;

    switch (config->algorithm) {
    case VGP_TILE_GOLDEN_RATIO:
        return tile_golden_ratio(area, window_count, gap,
                                  VGP_SPLIT_HORIZONTAL, out_rects);
    case VGP_TILE_EQUAL:
        return tile_equal(area, window_count, gap, out_rects);
    case VGP_TILE_MASTER_STACK:
        return tile_master_stack(area, window_count, gap,
                                  config->master_ratio, out_rects);
    case VGP_TILE_SPIRAL:
        return tile_spiral(area, window_count, gap, out_rects);
    }
    return 0;
}