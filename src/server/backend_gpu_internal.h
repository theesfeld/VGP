#ifndef VGP_BACKEND_GPU_INTERNAL_H
#define VGP_BACKEND_GPU_INTERNAL_H

#ifdef VGP_HAS_GPU_BACKEND

#include "render_backend.h"
#include "vgp/types.h"

#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include "nanovg.h"
#define NANOVG_GLES3 1
#include "nanovg_gl.h"

typedef struct vgp_gpu_output {
    struct gbm_surface *gbm_surface;
    EGLSurface          egl_surface;
    struct gbm_bo      *current_bo;
    struct gbm_bo      *prev_bo;
    uint32_t            prev_fb_id;
    bool                initialized;
} vgp_gpu_output_t;

typedef struct vgp_gpu_state {
    struct gbm_device *gbm_device;
    EGLDisplay         egl_display;
    EGLContext         egl_context;
    EGLConfig          egl_config;

    vgp_gpu_output_t   outputs[VGP_MAX_OUTPUTS];

    /* NanoVG context -- this IS the vector renderer */
    NVGcontext        *nvg;
    int                font_id;  /* NanoVG font handle */

    float              cur_width, cur_height;
    int                cur_output;

    /* Shader effects manager */
    void              *shader_mgr;  /* vgp_shader_mgr_t* (avoid circular include) */

    /* FBO compositing pipeline */
    GLuint             fbo;           /* framebuffer object for scene capture */
    GLuint             fbo_texture;   /* color attachment (scene rendered here) */
    GLuint             fbo_depth;     /* depth/stencil renderbuffer */
    GLuint             blur_program;  /* gaussian blur shader program */
    GLuint             blur_vao;
    GLuint             blur_vbo;
    uint32_t           fbo_width, fbo_height; /* current FBO dimensions */
    bool               fbo_initialized;
} vgp_gpu_state_t;

#endif /* VGP_HAS_GPU_BACKEND */
#endif /* VGP_BACKEND_GPU_INTERNAL_H */
