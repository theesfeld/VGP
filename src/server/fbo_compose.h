#ifndef VGP_FBO_COMPOSE_H
#define VGP_FBO_COMPOSE_H

#ifdef VGP_HAS_GPU_BACKEND

#include "backend_gpu_internal.h"

/* Initialize FBO + blur shader for compositing */
int vgp_fbo_init(vgp_gpu_state_t *gs, uint32_t width, uint32_t height);

/* Resize FBO if output dimensions changed */
int vgp_fbo_resize(vgp_gpu_state_t *gs, uint32_t width, uint32_t height);

/* Copy the current default framebuffer content into the FBO texture.
 * Call this after rendering the scene (after nvgEndFrame). */
void vgp_fbo_capture(vgp_gpu_state_t *gs, uint32_t width, uint32_t height);

/* Draw a blurred rectangle from the captured scene texture.
 * Used for plexiglass window backgrounds.
 * x,y,w,h = screen-space rectangle to blur.
 * radius = blur radius in pixels.
 * tint_r/g/b/a = color tint applied over the blur. */
void vgp_fbo_draw_blur_rect(vgp_gpu_state_t *gs,
                              float x, float y, float w, float h,
                              float corner_radius,
                              float blur_radius,
                              float tint_r, float tint_g, float tint_b, float tint_a,
                              uint32_t screen_w, uint32_t screen_h);

void vgp_fbo_destroy(vgp_gpu_state_t *gs);

#endif /* VGP_HAS_GPU_BACKEND */
#endif /* VGP_FBO_COMPOSE_H */
