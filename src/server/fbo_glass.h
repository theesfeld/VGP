/* SPDX-License-Identifier: MIT */
#ifndef VGP_FBO_GLASS_H
#define VGP_FBO_GLASS_H

/* FBO Glass Pipeline -- standard modern architecture.
 *
 * One-pass scene render to a full-resolution texture, two-stage
 * downsample + box blur chain (1/2, 1/4), then a per-window glass
 * fragment shader samples the scene + blur textures and applies:
 *   - Refraction offset (index-of-refraction style UV nudge)
 *   - Chromatic aberration proportional to edge distance
 *   - Fresnel edge brightness
 *   - Top-edge light scatter
 *   - Rounded-rect SDF clipping
 *   - Glass tint
 *
 * Phase separation: this entire pipeline lives between nvgEndFrame
 * and nvgBeginFrame. It does not interleave raw GL with NanoVG state. */

#ifdef VGP_HAS_GPU_BACKEND

#include "backend_gpu_internal.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t x, y;
    int32_t w, h;
    float   corner_radius;
    bool    focused;
} vgp_glass_rect_t;

/* Lifecycle */
int  vgp_fbo_glass_init(vgp_gpu_state_t *gs, uint32_t width, uint32_t height);
int  vgp_fbo_glass_resize(vgp_gpu_state_t *gs, uint32_t width, uint32_t height);
void vgp_fbo_glass_destroy(vgp_gpu_state_t *gs);

/* Enable / disable at runtime (env VGP_FBO toggles default) */
bool vgp_fbo_glass_enabled(const vgp_gpu_state_t *gs);

/* One-shot composite:
 *   1. Render scene_shader to scene_fbo (covers full viewport)
 *   2. Downsample scene_fbo -> blur_half -> blur_quarter (box blur)
 *   3. Blit scene_fbo to default FB (base layer)
 *   4. For each window rect: draw glass shader, sampling blur textures
 *      with IOR offset + chromatic aberration + Fresnel edge + tint
 *
 * Caller must have called nvgEndFrame() before this, and should call
 * nvgBeginFrame() after it resumes NanoVG drawing. */
void vgp_fbo_glass_composite(vgp_gpu_state_t *gs,
                               unsigned int scene_program,
                               float u_time,
                               uint32_t viewport_w, uint32_t viewport_h,
                               const vgp_glass_rect_t *rects, int rect_count,
                               float tint_r, float tint_g, float tint_b,
                               float tint_a,
                               float accent_r, float accent_g, float accent_b);

#endif /* VGP_HAS_GPU_BACKEND */
#endif /* VGP_FBO_GLASS_H */