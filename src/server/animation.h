#ifndef VGP_ANIMATION_H
#define VGP_ANIMATION_H

#include <stdbool.h>
#include <stdint.h>

#define VGP_MAX_ANIMATIONS 64

typedef enum {
    VGP_ANIM_NONE = 0,
    VGP_ANIM_WINDOW_OPEN,
    VGP_ANIM_WINDOW_CLOSE,
    VGP_ANIM_WINDOW_MINIMIZE,
    VGP_ANIM_WINDOW_MAXIMIZE,
    VGP_ANIM_WINDOW_RESTORE,
    VGP_ANIM_WORKSPACE_SLIDE,
} vgp_anim_type_t;

typedef enum {
    VGP_EASE_LINEAR,
    VGP_EASE_OUT_CUBIC,
    VGP_EASE_OUT_QUAD,
    VGP_EASE_IN_OUT_CUBIC,
} vgp_ease_t;

typedef struct vgp_animation {
    vgp_anim_type_t type;
    uint32_t        target_id;    /* window ID or output index */
    float           progress;     /* 0.0 to 1.0 */
    float           duration;     /* seconds */
    vgp_ease_t      easing;
    bool            active;

    /* Start/end values for interpolation */
    float           start_x, start_y, start_w, start_h;
    float           end_x, end_y, end_w, end_h;
    float           start_opacity, end_opacity;
    float           start_scale, end_scale;
} vgp_animation_t;

typedef struct vgp_animation_mgr {
    vgp_animation_t animations[VGP_MAX_ANIMATIONS];
    int             count;
    float           default_duration; /* configurable, default 0.2s */
    bool            enabled;          /* user can disable all animations */
} vgp_animation_mgr_t;

/* Apply easing function */
float vgp_ease(vgp_ease_t type, float t);

/* Animation manager */
void vgp_anim_init(vgp_animation_mgr_t *mgr, float duration, bool enabled);
void vgp_anim_tick(vgp_animation_mgr_t *mgr, float dt);

/* Start animations (returns animation pointer for querying) */
vgp_animation_t *vgp_anim_window_open(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                        float x, float y, float w, float h);
vgp_animation_t *vgp_anim_window_close(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                         float x, float y, float w, float h);
vgp_animation_t *vgp_anim_window_minimize(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                            float from_x, float from_y,
                                            float from_w, float from_h,
                                            float to_x, float to_y);

/* Workspace slide animation */
vgp_animation_t *vgp_anim_workspace_slide(vgp_animation_mgr_t *mgr,
                                            uint32_t output_idx,
                                            int direction); /* -1 = left, +1 = right */

/* Window maximize/restore animations */
vgp_animation_t *vgp_anim_window_maximize(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                            float from_x, float from_y,
                                            float from_w, float from_h,
                                            float to_x, float to_y,
                                            float to_w, float to_h);
vgp_animation_t *vgp_anim_window_restore(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                           float from_x, float from_y,
                                           float from_w, float from_h,
                                           float to_x, float to_y,
                                           float to_w, float to_h);

/* Query active animation for a window */
vgp_animation_t *vgp_anim_find(vgp_animation_mgr_t *mgr, uint32_t win_id);

/* Find workspace slide animation for an output */
vgp_animation_t *vgp_anim_find_ws_slide(vgp_animation_mgr_t *mgr, uint32_t output_idx);

/* Get interpolated values */
float vgp_anim_opacity(vgp_animation_t *a);
float vgp_anim_scale(vgp_animation_t *a);
void  vgp_anim_rect(vgp_animation_t *a, float *x, float *y, float *w, float *h);

/* Get workspace slide x offset (pixels) for the current transition.
 * Returns 0 if no slide active. */
float vgp_anim_ws_slide_offset(vgp_animation_t *a, float output_width);

#endif /* VGP_ANIMATION_H */
