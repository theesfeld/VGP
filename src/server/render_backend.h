/* SPDX-License-Identifier: MIT */
#ifndef VGP_RENDER_BACKEND_H
#define VGP_RENDER_BACKEND_H

#include "vgp/types.h"
#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct vgp_drm_output;

typedef enum {
    VGP_BACKEND_CPU,
    VGP_BACKEND_GPU,
} vgp_backend_type_t;

/* Opaque texture handle */
typedef struct vgp_texture {
    uint32_t id;
    uint32_t width;
    uint32_t height;
    void    *priv;
} vgp_texture_t;

typedef struct vgp_render_backend vgp_render_backend_t;

typedef struct vgp_render_ops {
    /* Lifecycle */
    int  (*init)(vgp_render_backend_t *b, int drm_fd);
    void (*destroy)(vgp_render_backend_t *b);

    /* Per-output framebuffer setup */
    int  (*output_init)(vgp_render_backend_t *b, int idx,
                        struct vgp_drm_output *output);
    void (*output_destroy)(vgp_render_backend_t *b, int idx);

    /* Frame lifecycle -- begin returns a backend-specific context pointer
     * (plutovg_canvas_t* for CPU, or NULL for GPU which uses GL state) */
    void *(*begin_frame)(vgp_render_backend_t *b, int output_idx,
                         struct vgp_drm_output *output);
    void  (*end_frame)(vgp_render_backend_t *b, int output_idx,
                       struct vgp_drm_output *output);

    /* Drawing primitives */
    void (*draw_rect)(vgp_render_backend_t *b, void *ctx,
                      float x, float y, float w, float h,
                      float r, float g, float ba, float a);
    void (*draw_rounded_rect)(vgp_render_backend_t *b, void *ctx,
                              float x, float y, float w, float h,
                              float radius,
                              float r, float g, float ba, float a);
    void (*draw_circle)(vgp_render_backend_t *b, void *ctx,
                        float cx, float cy, float rad,
                        float r, float g, float ba, float a);
    void (*draw_line)(vgp_render_backend_t *b, void *ctx,
                      float x1, float y1, float x2, float y2,
                      float width,
                      float r, float g, float ba, float a);
    void (*draw_text)(vgp_render_backend_t *b, void *ctx,
                      const char *text, int len, float x, float y,
                      float size,
                      float r, float g, float ba, float a);

    /* Texture operations (for client window surfaces) */
    vgp_texture_t *(*texture_create)(vgp_render_backend_t *b,
                                      uint32_t w, uint32_t h);
    void (*texture_destroy)(vgp_render_backend_t *b, vgp_texture_t *tex);
    void (*texture_upload)(vgp_render_backend_t *b, vgp_texture_t *tex,
                           const uint8_t *data, uint32_t stride);
    void (*draw_texture)(vgp_render_backend_t *b, void *ctx,
                         vgp_texture_t *tex,
                         float dx, float dy, float dw, float dh,
                         float alpha);

    /* Shader-driven rectangle (GPU only, no-op on CPU) */
    void (*draw_shader_rect)(vgp_render_backend_t *b, void *ctx,
                             int shader_id,
                             float x, float y, float w, float h,
                             float time);

    /* State management */
    void (*push_state)(vgp_render_backend_t *b, void *ctx);
    void (*pop_state)(vgp_render_backend_t *b, void *ctx);
    void (*set_clip)(vgp_render_backend_t *b, void *ctx,
                     float x, float y, float w, float h);

    /* Get the DRM fb_id for the current back buffer (for page flip) */
    uint32_t (*get_fb_id)(vgp_render_backend_t *b, int output_idx,
                          struct vgp_drm_output *output);

} vgp_render_ops_t;

struct vgp_render_backend {
    vgp_backend_type_t      type;
    const vgp_render_ops_t *ops;
    int                     drm_fd;
    void                   *priv;
};

/* Backend constructors */
vgp_render_backend_t *vgp_cpu_backend_create(void);

#ifdef VGP_HAS_GPU_BACKEND
vgp_render_backend_t *vgp_gpu_backend_create(void);
bool vgp_gpu_backend_available(int drm_fd);
#endif

#endif /* VGP_RENDER_BACKEND_H */