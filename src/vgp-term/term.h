#ifndef VGP_TERM_H
#define VGP_TERM_H

#include "vgp/vgp.h"
#include "vgp/protocol.h"

#include <vterm.h>
#include <stdbool.h>
#include <stdint.h>

#define VGP_TERM_MAX_SCROLLBACK 10000

/* Terminal configuration */
typedef struct vgp_term_config {
    char  shell[256];
    int   scrollback_lines;
    float font_size;
    int   cursor_style;       /* 1=block, 2=underline, 3=bar */
    bool  cursor_blink;
    int   cursor_blink_ms;
    int   padding;

    /* Color overrides (0 = use defaults) */
    uint32_t fg_override;     /* 0xRRGGBB or 0 */
    uint32_t bg_override;
    uint32_t cursor_color;
    uint32_t selection_bg;
    uint32_t selection_fg;
} vgp_term_config_t;

/* Font metrics (for computing grid dimensions -- actual rendering is server-side) */
typedef struct vgp_term_fonts {
    float size;
    float cell_width;
    float cell_height;
    float ascent;
} vgp_term_fonts_t;

/* Cursor state */
typedef struct vgp_term_cursor {
    VTermPos pos;
    bool     visible;
    bool     blink_state;
    int      shape;
} vgp_term_cursor_t;

/* Dirty tracking */
typedef struct vgp_term_damage {
    bool *dirty_rows;
    bool  full_redraw;
    int   dirty_count;
} vgp_term_damage_t;

/* Color palette for resolving VTermColor -> RGB */
typedef struct vgp_term_color {
    float r, g, b, a;
} vgp_term_color_t;

/* Main terminal state */
typedef struct vgp_term {
    /* VGP connection */
    vgp_connection_t *conn;
    uint32_t          window_id;

    /* Grid dimensions */
    int                rows;
    int                cols;

    /* libvterm */
    VTerm       *vt;
    VTermScreen *vt_screen;

    /* PTY */
    int   pty_fd;
    pid_t child_pid;

    /* Subsystems */
    vgp_term_fonts_t  fonts;
    vgp_term_cursor_t cursor;
    vgp_term_damage_t damage;

    /* Throttling */
    bool     render_pending;

    /* Color palette (258 entries: 256 indexed + default fg + default bg) */
    vgp_term_color_t palette[258];

    /* Configuration */
    vgp_term_config_t config;

    /* Scrollback buffer */
    struct {
        VTermScreenCell **lines; /* ring buffer of saved lines */
        int              *line_cols;
        int               capacity;
        int               count;
        int               head;
        int               scroll_offset; /* 0 = bottom (live), >0 = scrolled up */
    } scrollback;

    /* Selection */
    struct {
        int  start_row, start_col;
        int  end_row, end_col;
        bool active;      /* mouse drag in progress */
        bool valid;        /* completed selection exists */
        char *text;        /* selected text UTF-8, malloc'd */
        size_t text_len;
    } selection;

    /* Running state */
    bool running;
} vgp_term_t;

/* Lifecycle */
int  vgp_term_init(vgp_term_t *term, int width, int height);
void vgp_term_destroy(vgp_term_t *term);
void vgp_term_run(vgp_term_t *term);

/* PTY */
int     vgp_term_pty_spawn(vgp_term_t *term);
int     vgp_term_pty_resize(vgp_term_t *term, int rows, int cols);
ssize_t vgp_term_pty_write(vgp_term_t *term, const char *data, size_t len);

/* Rendering (now sends cell grid, no local pixel rendering) */
void vgp_term_render_dirty(vgp_term_t *term);
void vgp_term_render_row(vgp_term_t *term, int row);
void vgp_term_render_cursor(vgp_term_t *term);
void vgp_term_commit_surface(vgp_term_t *term);

/* Color */
void vgp_term_palette_init(vgp_term_t *term);

/* Font metrics */
int  vgp_term_fonts_init(vgp_term_fonts_t *fonts, float size);
void vgp_term_fonts_destroy(vgp_term_fonts_t *fonts);

#endif /* VGP_TERM_H */
