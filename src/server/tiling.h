#ifndef VGP_TILING_H
#define VGP_TILING_H

#include "vgp/types.h"
#include <stdbool.h>

#define VGP_TILE_MAX_WINDOWS 64

typedef enum {
    VGP_TILE_GOLDEN_RATIO,  /* first window gets golden ratio, rest split remaining */
    VGP_TILE_EQUAL,         /* all windows get equal space */
    VGP_TILE_MASTER_STACK,  /* one master on left, rest stacked on right */
    VGP_TILE_SPIRAL,        /* fibonacci spiral layout */
} vgp_tile_algorithm_t;

typedef enum {
    VGP_SPLIT_HORIZONTAL,   /* split left/right */
    VGP_SPLIT_VERTICAL,     /* split top/bottom */
} vgp_split_dir_t;

typedef struct vgp_tile_config {
    vgp_tile_algorithm_t algorithm;
    float  master_ratio;     /* for master+stack: ratio of master (0.5 = 50%) */
    int    gap_inner;        /* pixels between windows */
    int    gap_outer;        /* pixels between windows and screen edge */
    bool   smart_gaps;       /* hide gaps when only 1 window */
} vgp_tile_config_t;

/* Calculate tiled positions for a set of windows within a given area.
 * out_rects: array of VGP_TILE_MAX_WINDOWS rects to fill
 * Returns number of rects calculated. */
int vgp_tile_calculate(vgp_tile_config_t *config,
                        int window_count,
                        vgp_rect_t area,  /* usable screen area (minus panel) */
                        vgp_rect_t *out_rects);

/* Parse algorithm name from config string */
vgp_tile_algorithm_t vgp_tile_parse_algorithm(const char *name);

/* Get algorithm name */
const char *vgp_tile_algorithm_name(vgp_tile_algorithm_t algo);

#endif /* VGP_TILING_H */
