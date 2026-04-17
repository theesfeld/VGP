/* SPDX-License-Identifier: MIT */
#ifndef VGP_RENDERER_H
#define VGP_RENDERER_H

#include "drm.h"
#include "render_backend.h"
#include "compositor.h"
#include "theme.h"
#include "timer.h"

typedef struct vgp_renderer {
    vgp_render_backend_t *backend;
    vgp_timer_t           frame_timer;
    bool                  frame_scheduled;
    bool                  dirty;
    bool                  gpu_crtc_owned[VGP_MAX_OUTPUTS];

    /* Shader effect indices (-1 = no shader, use solid color) */
    int                   shader_background;
    int                   shader_titlebar;
    int                   shader_panel;
    int                   shader_overlay;   /* post-process overlay (rain, etc.) */

    /* Accessibility */
    bool                  focus_indicator;  /* draw bright ring around focused window */
    float                 text_size;        /* override vector text render size (0 = theme default) */
    bool                  large_cursor;     /* 2x cursor size */
} vgp_renderer_t;

/* Forward declaration */
struct vgp_server;

int  vgp_renderer_init(vgp_renderer_t *renderer, vgp_drm_backend_t *drm,
                        vgp_event_loop_t *loop, struct vgp_server *server);
void vgp_renderer_destroy(vgp_renderer_t *renderer, vgp_event_loop_t *loop);

/* Mark the scene as needing a repaint + schedule frame */
void vgp_renderer_schedule_frame(vgp_renderer_t *renderer);

/* Render one output (full repaint) */
struct vgp_notify;
struct vgp_animation_mgr;
struct vgp_lockscreen;
struct vgp_menu;
struct vgp_calendar;
struct vgp_config_panel;

void vgp_renderer_render_output(vgp_renderer_t *renderer,
                                 vgp_drm_backend_t *drm,
                                 vgp_drm_output_t *output,
                                 int output_idx,
                                 vgp_compositor_t *comp,
                                 vgp_theme_t *theme,
                                 struct vgp_notify *notify,
                                 struct vgp_animation_mgr *anims,
                                 struct vgp_lockscreen *lock,
                                 struct vgp_menu *menu,
                                 struct vgp_calendar *cal,
                                 const struct vgp_config_panel *panel_cfg);

#endif /* VGP_RENDERER_H */