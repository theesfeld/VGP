#ifndef VGP_COMPOSITOR_H
#define VGP_COMPOSITOR_H

#include "window.h"
#include "cursor.h"
#include "theme.h"

/* Forward declarations */
struct vgp_server;
struct vgp_renderer;

#define VGP_MAX_WORKSPACES 9

typedef struct vgp_grab {
    vgp_window_t   *target;
    vgp_hit_region_t region;
    int32_t          grab_x, grab_y;
    vgp_rect_t       grab_rect;
    bool             active;
} vgp_grab_t;

typedef struct vgp_output_info {
    int32_t  x, y;           /* position in global coordinate space */
    uint32_t width, height;
    int      workspace;      /* which workspace this output displays */
    bool     active;
} vgp_output_info_t;

typedef struct vgp_compositor {
    vgp_window_t    windows[VGP_MAX_WINDOWS];
    int             window_count;

    /* Z-order: sorted array of pointers, bottom-to-top */
    vgp_window_t   *z_order[VGP_MAX_WINDOWS];

    /* Currently focused window */
    vgp_window_t   *focused;

    /* Mouse grab state */
    vgp_grab_t      grab;

    /* Cursor */
    vgp_cursor_t    cursor;

    /* Outputs and workspaces */
    vgp_output_info_t outputs[VGP_MAX_OUTPUTS];
    int               output_count;
    int               active_output;

    /* Expose/overview mode */
    bool              expose_active;
    vgp_rect_t        expose_rects[VGP_MAX_WINDOWS]; /* tile positions for expose */
} vgp_compositor_t;

int  vgp_compositor_init(vgp_compositor_t *comp);
void vgp_compositor_destroy(vgp_compositor_t *comp);

/* Configure outputs (call after DRM enumeration) */
void vgp_compositor_set_outputs(vgp_compositor_t *comp,
                                 int count,
                                 const uint32_t *widths,
                                 const uint32_t *heights);

/* Window lifecycle */
vgp_window_t *vgp_compositor_create_window(vgp_compositor_t *comp,
                                            int client_fd,
                                            int32_t x, int32_t y,
                                            uint32_t w, uint32_t h,
                                            const char *title,
                                            const vgp_theme_t *theme);
void vgp_compositor_destroy_window(vgp_compositor_t *comp,
                                    vgp_window_t *win);

/* Focus and z-order */
void vgp_compositor_focus_window(vgp_compositor_t *comp,
                                  vgp_window_t *win);
void vgp_compositor_raise_window(vgp_compositor_t *comp,
                                  vgp_window_t *win);

/* Find window at screen coordinates ON A SPECIFIC WORKSPACE */
vgp_window_t *vgp_compositor_window_at(vgp_compositor_t *comp,
                                        int32_t x, int32_t y);

/* Window state changes */
void vgp_compositor_minimize_window(vgp_compositor_t *comp,
                                     vgp_window_t *win);
void vgp_compositor_maximize_window(vgp_compositor_t *comp,
                                     vgp_window_t *win,
                                     uint32_t output_w, uint32_t output_h,
                                     const vgp_theme_t *theme);
void vgp_compositor_restore_window(vgp_compositor_t *comp,
                                    vgp_window_t *win);
void vgp_compositor_move_window(vgp_compositor_t *comp,
                                 vgp_window_t *win,
                                 int32_t x, int32_t y,
                                 const vgp_theme_t *theme);
void vgp_compositor_resize_window(vgp_compositor_t *comp,
                                   vgp_window_t *win,
                                   uint32_t w, uint32_t h,
                                   const vgp_theme_t *theme);

/* Cycle focus among visible windows on the current workspace */
void vgp_compositor_focus_cycle(vgp_compositor_t *comp, int direction);

/* Re-tile all windows on a workspace according to the tiling config */
struct vgp_tile_config;
void vgp_compositor_retile(vgp_compositor_t *comp, int workspace,
                            struct vgp_tile_config *tile_config,
                            const vgp_theme_t *theme);

/* Determine which output the cursor is on */
int vgp_compositor_output_at_cursor(vgp_compositor_t *comp);

#endif /* VGP_COMPOSITOR_H */
