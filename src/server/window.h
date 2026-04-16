#ifndef VGP_WINDOW_H
#define VGP_WINDOW_H

#include "vgp/types.h"
#include "theme.h"
#include <plutovg.h>

typedef uint32_t vgp_window_id_t;

typedef enum {
    VGP_WIN_NORMAL,
    VGP_WIN_MINIMIZED,
    VGP_WIN_MAXIMIZED,
    VGP_WIN_FULLSCREEN,
} vgp_window_state_t;

typedef enum {
    VGP_HIT_NONE,
    VGP_HIT_TITLEBAR,
    VGP_HIT_CLOSE_BTN,
    VGP_HIT_MAXIMIZE_BTN,
    VGP_HIT_MINIMIZE_BTN,
    VGP_HIT_BORDER_N,
    VGP_HIT_BORDER_S,
    VGP_HIT_BORDER_E,
    VGP_HIT_BORDER_W,
    VGP_HIT_BORDER_NE,
    VGP_HIT_BORDER_NW,
    VGP_HIT_BORDER_SE,
    VGP_HIT_BORDER_SW,
    VGP_HIT_CONTENT,
} vgp_hit_region_t;

typedef struct vgp_window {
    vgp_window_id_t    id;
    vgp_window_state_t state;
    bool               used;

    /* Geometry: frame includes decorations, content is inside */
    vgp_rect_t         frame_rect;
    vgp_rect_t         content_rect;
    vgp_rect_t         saved_rect;   /* pre-maximize geometry */

    /* Client surface */
    plutovg_surface_t *client_surface;
    uint32_t           client_width;
    uint32_t           client_height;

    /* Metadata */
    char               title[VGP_MAX_TITLE_LEN];
    bool               focused;
    bool               visible;
    bool               decorated;

    /* IPC client that owns this window */
    int                client_fd;

    /* Z-order index (managed by compositor) */
    int                z_index;

    /* Workspace this window belongs to (0-based) */
    int                workspace;
    bool               floating_override; /* true = always float even in tiling mode */

    /* GPU: persistent NanoVG image handle for pixel surfaces (-1 = none) */
    int                nvg_image;
    uint32_t           nvg_image_w, nvg_image_h;

    /* Vector terminal: cell grid data (server renders text) */
    void              *cellgrid;       /* vgp_cell_t array */
    uint16_t           grid_rows;
    uint16_t           grid_cols;
    uint16_t           cursor_row, cursor_col;
    uint8_t            cursor_visible;
    uint8_t            cursor_shape;
    bool               has_cellgrid;
    float              font_size_override; /* 0 = use default, >0 = client requested */

    /* Draw commands (graphical UI protocol) */
    void              *draw_cmds;        /* packed command stream */
    size_t             draw_cmds_len;    /* byte length of command data */
    uint32_t           draw_cmd_count;
    bool               has_drawcmds;
} vgp_window_t;

/* Compute content rect from frame rect using theme decoration sizes */
vgp_rect_t vgp_window_content_rect(const vgp_rect_t *frame,
                                     const vgp_theme_t *theme);

/* Compute frame rect from desired content position and size */
vgp_rect_t vgp_window_frame_rect(int32_t cx, int32_t cy,
                                   uint32_t cw, uint32_t ch,
                                   const vgp_theme_t *theme);

/* Hit-test a point against window regions */
vgp_hit_region_t vgp_window_hit_test(const vgp_window_t *win,
                                      const vgp_theme_t *theme,
                                      int32_t x, int32_t y);

#endif /* VGP_WINDOW_H */
