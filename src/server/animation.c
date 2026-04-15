#include "animation.h"
#include <string.h>
#include <math.h>

float vgp_ease(vgp_ease_t type, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    switch (type) {
    case VGP_EASE_LINEAR:
        return t;
    case VGP_EASE_OUT_CUBIC: {
        float p = 1.0f - t;
        return 1.0f - p * p * p;
    }
    case VGP_EASE_OUT_QUAD:
        return 1.0f - (1.0f - t) * (1.0f - t);
    case VGP_EASE_IN_OUT_CUBIC:
        return t < 0.5f
            ? 4.0f * t * t * t
            : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
    }
    return t;
}

void vgp_anim_init(vgp_animation_mgr_t *mgr, float duration, bool enabled)
{
    memset(mgr, 0, sizeof(*mgr));
    mgr->default_duration = duration > 0 ? duration : 0.2f;
    mgr->enabled = enabled;
}

void vgp_anim_tick(vgp_animation_mgr_t *mgr, float dt)
{
    for (int i = 0; i < VGP_MAX_ANIMATIONS; i++) {
        vgp_animation_t *a = &mgr->animations[i];
        if (!a->active) continue;

        a->progress += dt / a->duration;
        if (a->progress >= 1.0f) {
            a->progress = 1.0f;
            a->active = false;
            mgr->count--;
        }
    }
}

static vgp_animation_t *alloc_anim(vgp_animation_mgr_t *mgr)
{
    for (int i = 0; i < VGP_MAX_ANIMATIONS; i++) {
        if (!mgr->animations[i].active) {
            mgr->count++;
            return &mgr->animations[i];
        }
    }
    /* Evict oldest */
    float oldest_progress = 0;
    int oldest_idx = 0;
    for (int i = 0; i < VGP_MAX_ANIMATIONS; i++) {
        if (mgr->animations[i].progress > oldest_progress) {
            oldest_progress = mgr->animations[i].progress;
            oldest_idx = i;
        }
    }
    return &mgr->animations[oldest_idx];
}

vgp_animation_t *vgp_anim_window_open(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                        float x, float y, float w, float h)
{
    if (!mgr->enabled) return NULL;

    /* Cancel any existing animation for this window */
    vgp_animation_t *existing = vgp_anim_find(mgr, win_id);
    if (existing) { existing->active = false; mgr->count--; }

    vgp_animation_t *a = alloc_anim(mgr);
    memset(a, 0, sizeof(*a));
    a->type = VGP_ANIM_WINDOW_OPEN;
    a->target_id = win_id;
    a->duration = mgr->default_duration;
    a->easing = VGP_EASE_OUT_CUBIC;
    a->active = true;
    a->progress = 0.0f;

    /* Scale from 0.92 to 1.0, opacity from 0 to 1 */
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float scale0 = 0.92f;
    a->start_x = cx - w * scale0 * 0.5f;
    a->start_y = cy - h * scale0 * 0.5f;
    a->start_w = w * scale0;
    a->start_h = h * scale0;
    a->end_x = x; a->end_y = y;
    a->end_w = w; a->end_h = h;
    a->start_opacity = 0.0f;
    a->end_opacity = 1.0f;
    a->start_scale = scale0;
    a->end_scale = 1.0f;

    return a;
}

vgp_animation_t *vgp_anim_window_close(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                         float x, float y, float w, float h)
{
    if (!mgr->enabled) return NULL;

    vgp_animation_t *existing = vgp_anim_find(mgr, win_id);
    if (existing) { existing->active = false; mgr->count--; }

    vgp_animation_t *a = alloc_anim(mgr);
    memset(a, 0, sizeof(*a));
    a->type = VGP_ANIM_WINDOW_CLOSE;
    a->target_id = win_id;
    a->duration = mgr->default_duration * 0.8f;
    a->easing = VGP_EASE_OUT_QUAD;
    a->active = true;
    a->progress = 0.0f;

    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float scale1 = 0.92f;
    a->start_x = x; a->start_y = y;
    a->start_w = w; a->start_h = h;
    a->end_x = cx - w * scale1 * 0.5f;
    a->end_y = cy - h * scale1 * 0.5f;
    a->end_w = w * scale1;
    a->end_h = h * scale1;
    a->start_opacity = 1.0f;
    a->end_opacity = 0.0f;
    a->start_scale = 1.0f;
    a->end_scale = scale1;

    return a;
}

vgp_animation_t *vgp_anim_window_minimize(vgp_animation_mgr_t *mgr, uint32_t win_id,
                                            float from_x, float from_y,
                                            float from_w, float from_h,
                                            float to_x, float to_y)
{
    if (!mgr->enabled) return NULL;

    vgp_animation_t *existing = vgp_anim_find(mgr, win_id);
    if (existing) { existing->active = false; mgr->count--; }

    vgp_animation_t *a = alloc_anim(mgr);
    memset(a, 0, sizeof(*a));
    a->type = VGP_ANIM_WINDOW_MINIMIZE;
    a->target_id = win_id;
    a->duration = mgr->default_duration;
    a->easing = VGP_EASE_IN_OUT_CUBIC;
    a->active = true;
    a->progress = 0.0f;

    a->start_x = from_x; a->start_y = from_y;
    a->start_w = from_w; a->start_h = from_h;
    a->end_x = to_x; a->end_y = to_y;
    a->end_w = 0; a->end_h = 0;
    a->start_opacity = 1.0f;
    a->end_opacity = 0.0f;
    a->start_scale = 1.0f;
    a->end_scale = 0.0f;

    return a;
}

vgp_animation_t *vgp_anim_find(vgp_animation_mgr_t *mgr, uint32_t win_id)
{
    for (int i = 0; i < VGP_MAX_ANIMATIONS; i++) {
        if (mgr->animations[i].active && mgr->animations[i].target_id == win_id)
            return &mgr->animations[i];
    }
    return NULL;
}

float vgp_anim_opacity(vgp_animation_t *a)
{
    if (!a) return 1.0f;
    float t = vgp_ease(a->easing, a->progress);
    return a->start_opacity + (a->end_opacity - a->start_opacity) * t;
}

float vgp_anim_scale(vgp_animation_t *a)
{
    if (!a) return 1.0f;
    float t = vgp_ease(a->easing, a->progress);
    return a->start_scale + (a->end_scale - a->start_scale) * t;
}

void vgp_anim_rect(vgp_animation_t *a, float *x, float *y, float *w, float *h)
{
    if (!a) return;
    float t = vgp_ease(a->easing, a->progress);
    *x = a->start_x + (a->end_x - a->start_x) * t;
    *y = a->start_y + (a->end_y - a->start_y) * t;
    *w = a->start_w + (a->end_w - a->start_w) * t;
    *h = a->start_h + (a->end_h - a->start_h) * t;
}
