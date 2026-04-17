/* SPDX-License-Identifier: MIT */
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

    /* FBO glass pipeline */
    GLuint    scene_fbo,  scene_tex;   /* full-res scene render target */
    GLuint    blur_h_fbo, blur_h_tex;  /* half-res downsample */
    GLuint    blur_q_fbo, blur_q_tex;  /* quarter-res downsample */
    uint32_t  scene_w,  scene_h;
    uint32_t  blur_hw, blur_hh;
    uint32_t  blur_qw, blur_qh;
    GLuint    gl_prog_passthrough;
    GLuint    gl_prog_downsample;
    GLuint    gl_prog_glass;
    GLuint    gl_quad_vao, gl_quad_vbo;
    bool      fbo_enabled;             /* true once FBOs are ready */
} vgp_gpu_state_t;

#endif /* VGP_HAS_GPU_BACKEND */
#endif /* VGP_BACKEND_GPU_INTERNAL_H */