/* SPDX-License-Identifier: MIT */
#ifdef VGP_HAS_GPU_BACKEND

#include "backend_gpu_internal.h"
#include "render_backend.h"
#include "drm.h"
#include "vgp/log.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TAG "gpu"

/* ============================================================
 * GBM BO -> DRM framebuffer
 * ============================================================ */

static uint32_t bo_to_fb(int drm_fd, struct gbm_bo *bo)
{
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t stride = gbm_bo_get_stride(bo);
    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t fb_id = 0;

    if (drmModeAddFB(drm_fd, width, height, 24, 32, stride, handle, &fb_id) < 0)
        VGP_LOG_ERROR(TAG, "drmModeAddFB failed: %s", strerror(errno));
    return fb_id;
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

static int gpu_init(vgp_render_backend_t *b, int drm_fd)
{
    vgp_gpu_state_t *s = calloc(1, sizeof(*s));
    if (!s) return -1;
    b->priv = s;
    b->drm_fd = drm_fd;

    s->gbm_device = gbm_create_device(drm_fd);
    if (!s->gbm_device) {
        VGP_LOG_ERROR(TAG, "gbm_create_device failed");
        goto err;
    }

    s->egl_display = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR,
                                            s->gbm_device, NULL);
    if (s->egl_display == EGL_NO_DISPLAY) {
        VGP_LOG_ERROR(TAG, "eglGetPlatformDisplay failed");
        goto err_gbm;
    }

    EGLint major, minor;
    if (!eglInitialize(s->egl_display, &major, &minor)) {
        VGP_LOG_ERROR(TAG, "eglInitialize failed");
        goto err_gbm;
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE,
    };
    EGLint n;
    if (!eglChooseConfig(s->egl_display, cfg_attribs, &s->egl_config, 1, &n) || n == 0) {
        VGP_LOG_ERROR(TAG, "eglChooseConfig failed");
        goto err_egl;
    }

    EGLint ctx_attribs[] = { EGL_CONTEXT_MAJOR_VERSION, 3,
                              EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE };
    s->egl_context = eglCreateContext(s->egl_display, s->egl_config,
                                       EGL_NO_CONTEXT, ctx_attribs);
    if (s->egl_context == EGL_NO_CONTEXT) {
        VGP_LOG_ERROR(TAG, "eglCreateContext failed: 0x%x", eglGetError());
        goto err_egl;
    }

    VGP_LOG_INFO(TAG, "EGL %d.%d + GLES3 initialized", major, minor);
    return 0;

err_egl:
    eglTerminate(s->egl_display);
err_gbm:
    gbm_device_destroy(s->gbm_device);
err:
    free(s);
    b->priv = NULL;
    return -1;
}

static void gpu_destroy(vgp_render_backend_t *b)
{
    vgp_gpu_state_t *s = b->priv;
    if (!s) return;

    if (s->nvg) nvgDeleteGLES3(s->nvg);

    eglMakeCurrent(s->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    for (int i = 0; i < VGP_MAX_OUTPUTS; i++) {
        vgp_gpu_output_t *out = &s->outputs[i];
        if (out->prev_bo) {
            if (out->prev_fb_id) drmModeRmFB(b->drm_fd, out->prev_fb_id);
            gbm_surface_release_buffer(out->gbm_surface, out->prev_bo);
        }
        if (out->egl_surface != EGL_NO_SURFACE)
            eglDestroySurface(s->egl_display, out->egl_surface);
        if (out->gbm_surface)
            gbm_surface_destroy(out->gbm_surface);
    }

    eglDestroyContext(s->egl_display, s->egl_context);
    eglTerminate(s->egl_display);
    gbm_device_destroy(s->gbm_device);
    free(s);
    b->priv = NULL;
}

static int gpu_output_init(vgp_render_backend_t *b, int idx,
                            struct vgp_drm_output *output)
{
    vgp_gpu_state_t *s = b->priv;
    if (idx < 0 || idx >= VGP_MAX_OUTPUTS) return -1;
    vgp_gpu_output_t *out = &s->outputs[idx];

    out->gbm_surface = gbm_surface_create(s->gbm_device,
                                            output->width, output->height,
                                            GBM_FORMAT_ARGB8888,
                                            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!out->gbm_surface) {
        VGP_LOG_ERROR(TAG, "gbm_surface_create failed");
        return -1;
    }

    out->egl_surface = eglCreatePlatformWindowSurface(s->egl_display,
                                                       s->egl_config,
                                                       out->gbm_surface, NULL);
    if (out->egl_surface == EGL_NO_SURFACE) {
        VGP_LOG_ERROR(TAG, "eglCreatePlatformWindowSurface failed: 0x%x", eglGetError());
        gbm_surface_destroy(out->gbm_surface);
        out->gbm_surface = NULL;
        return -1;
    }

    /* Create NanoVG context on first output (needs active EGL context) */
    if (!s->nvg) {
        eglMakeCurrent(s->egl_display, out->egl_surface, out->egl_surface,
                        s->egl_context);

        s->nvg = nvgCreateGLES3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
        if (!s->nvg) {
            VGP_LOG_ERROR(TAG, "nvgCreateGLES3 failed");
            return -1;
        }

        /* Load font -- NanoVG renders text as GPU vector paths via stb_truetype */
        const char *font_paths[] = {
            "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/TTF/Hack-Regular.ttf",
            "/usr/share/fonts/noto/NotoSansMono-Regular.ttf",
            NULL,
        };
        s->font_id = -1;
        for (const char **p = font_paths; *p; p++) {
            s->font_id = nvgCreateFont(s->nvg, "default", *p);
            if (s->font_id >= 0) {
                VGP_LOG_INFO(TAG, "loaded font: %s", *p);
                break;
            }
        }

        VGP_LOG_INFO(TAG, "NanoVG GLES3 context created");
    }

    out->initialized = true;
    VGP_LOG_INFO(TAG, "output %d: %ux%u ready", idx, output->width, output->height);
    return 0;
}

static void gpu_output_destroy(vgp_render_backend_t *b, int idx)
{
    vgp_gpu_state_t *s = b->priv;
    if (idx < 0 || idx >= VGP_MAX_OUTPUTS) return;
    vgp_gpu_output_t *out = &s->outputs[idx];
    if (out->prev_bo) {
        if (out->prev_fb_id) drmModeRmFB(b->drm_fd, out->prev_fb_id);
        gbm_surface_release_buffer(out->gbm_surface, out->prev_bo);
    }
    if (out->egl_surface) eglDestroySurface(s->egl_display, out->egl_surface);
    if (out->gbm_surface) gbm_surface_destroy(out->gbm_surface);
    memset(out, 0, sizeof(*out));
}

/* ============================================================
 * Frame lifecycle
 * ============================================================ */

static void *gpu_begin_frame(vgp_render_backend_t *b, int output_idx,
                              struct vgp_drm_output *output)
{
    vgp_gpu_state_t *s = b->priv;
    vgp_gpu_output_t *out = &s->outputs[output_idx];
    if (!out->initialized || !s->nvg) return NULL;

    eglMakeCurrent(s->egl_display, out->egl_surface, out->egl_surface,
                    s->egl_context);

    s->cur_width = (float)output->width;
    s->cur_height = (float)output->height;
    s->cur_output = output_idx;

    glViewport(0, 0, (int)output->width, (int)output->height);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(s->nvg, s->cur_width, s->cur_height, 1.0f);

    return s->nvg;  /* context IS the NanoVG context */
}

static void gpu_end_frame(vgp_render_backend_t *b, int output_idx,
                           struct vgp_drm_output *output)
{
    vgp_gpu_state_t *s = b->priv;
    vgp_gpu_output_t *out = &s->outputs[output_idx];
    (void)output;

    nvgEndFrame(s->nvg);
    eglSwapBuffers(s->egl_display, out->egl_surface);

    struct gbm_bo *bo = gbm_surface_lock_front_buffer(out->gbm_surface);
    if (!bo) {
        VGP_LOG_ERROR(TAG, "lock_front_buffer failed for output %d", output_idx);
        return;
    }

    uint32_t new_fb_id = bo_to_fb(b->drm_fd, bo);

    /* Release the BO from 2 frames ago (prev_bo).
     * current_bo is the one SetCrtc'd last frame (still on screen until
     * the renderer calls SetCrtc with the new fb after we return).
     * prev_bo was on screen 2 frames ago -- definitely safe to free. */
    if (out->prev_bo) {
        gbm_surface_release_buffer(out->gbm_surface, out->prev_bo);
    }

    out->prev_bo = out->current_bo;
    out->current_bo = bo;
    out->prev_fb_id = new_fb_id;
}

static uint32_t gpu_get_fb_id(vgp_render_backend_t *b, int output_idx,
                               struct vgp_drm_output *output)
{
    (void)output;
    vgp_gpu_state_t *s = b->priv;
    return s->outputs[output_idx].prev_fb_id;
}

/* ============================================================
 * Drawing -- all via NanoVG (true GPU vector rendering)
 * ============================================================ */

static void gpu_draw_rect(vgp_render_backend_t *b, void *ctx,
                           float x, float y, float w, float h,
                           float r, float g, float ba, float a)
{
    (void)b;
    NVGcontext *vg = ctx;
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGBAf(r, g, ba, a));
    nvgFill(vg);
}

static void gpu_draw_rounded_rect(vgp_render_backend_t *b, void *ctx,
                                   float x, float y, float w, float h,
                                   float radius,
                                   float r, float g, float ba, float a)
{
    (void)b;
    NVGcontext *vg = ctx;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, radius);
    nvgFillColor(vg, nvgRGBAf(r, g, ba, a));
    nvgFill(vg);
}

static void gpu_draw_circle(vgp_render_backend_t *b, void *ctx,
                             float cx, float cy, float rad,
                             float r, float g, float ba, float a)
{
    (void)b;
    NVGcontext *vg = ctx;
    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, rad);
    nvgFillColor(vg, nvgRGBAf(r, g, ba, a));
    nvgFill(vg);
}

static void gpu_draw_line(vgp_render_backend_t *b, void *ctx,
                           float x1, float y1, float x2, float y2,
                           float width,
                           float r, float g, float ba, float a)
{
    (void)b;
    NVGcontext *vg = ctx;
    nvgBeginPath(vg);
    nvgMoveTo(vg, x1, y1);
    nvgLineTo(vg, x2, y2);
    nvgStrokeColor(vg, nvgRGBAf(r, g, ba, a));
    nvgStrokeWidth(vg, width);
    nvgStroke(vg);
}

static void gpu_draw_text(vgp_render_backend_t *b, void *ctx,
                           const char *text, int len, float x, float y,
                           float size,
                           float r, float g, float ba, float a)
{
    vgp_gpu_state_t *s = b->priv;
    if (s->font_id < 0) return;
    NVGcontext *vg = ctx;
    nvgFontFaceId(vg, s->font_id);
    nvgFontSize(vg, size);
    nvgFillColor(vg, nvgRGBAf(r, g, ba, a));
    if (len < 0)
        nvgText(vg, x, y, text, NULL);
    else
        nvgText(vg, x, y, text, text + len);
}

/* ============================================================
 * Textures (for client window surfaces)
 * ============================================================ */

typedef struct vgp_gpu_texture {
    int nvg_image;  /* NanoVG image handle */
} vgp_gpu_texture_t;

static vgp_texture_t *gpu_texture_create(vgp_render_backend_t *b,
                                          uint32_t w, uint32_t h)
{
    vgp_gpu_state_t *s = b->priv;
    vgp_texture_t *tex = calloc(1, sizeof(*tex));
    vgp_gpu_texture_t *gt = calloc(1, sizeof(*gt));

    gt->nvg_image = nvgCreateImageRGBA(s->nvg, (int)w, (int)h, 0, NULL);
    tex->width = w;
    tex->height = h;
    tex->priv = gt;
    return tex;
}

static void gpu_texture_destroy(vgp_render_backend_t *b, vgp_texture_t *tex)
{
    if (!tex) return;
    vgp_gpu_state_t *s = b->priv;
    vgp_gpu_texture_t *gt = tex->priv;
    if (gt) {
        if (gt->nvg_image) nvgDeleteImage(s->nvg, gt->nvg_image);
        free(gt);
    }
    free(tex);
}

static void gpu_texture_upload(vgp_render_backend_t *b, vgp_texture_t *tex,
                                const uint8_t *data, uint32_t stride)
{
    vgp_gpu_state_t *s = b->priv;
    vgp_gpu_texture_t *gt = tex->priv;
    (void)stride; /* NanoVG assumes packed RGBA */
    nvgUpdateImage(s->nvg, gt->nvg_image, data);
}

static void gpu_draw_texture(vgp_render_backend_t *b, void *ctx,
                              vgp_texture_t *tex,
                              float dx, float dy, float dw, float dh,
                              float alpha)
{
    vgp_gpu_state_t *s = b->priv;
    vgp_gpu_texture_t *gt = tex->priv;
    NVGcontext *vg = ctx;
    (void)b;

    NVGpaint paint = nvgImagePattern(vg, dx, dy, dw, dh, 0, gt->nvg_image, alpha);
    nvgBeginPath(vg);
    nvgRect(vg, dx, dy, dw, dh);
    nvgFillPaint(vg, paint);
    nvgFill(vg);
}

/* ============================================================
 * State
 * ============================================================ */

static void gpu_draw_shader_rect(vgp_render_backend_t *b, void *ctx,
                                  int shader_id, float x, float y,
                                  float w, float h, float time)
{
    (void)b; (void)ctx; (void)shader_id;
    (void)x; (void)y; (void)w; (void)h; (void)time;
}

static void gpu_push_state(vgp_render_backend_t *b, void *ctx)
{
    (void)b;
    nvgSave(ctx);
}

static void gpu_pop_state(vgp_render_backend_t *b, void *ctx)
{
    (void)b;
    nvgRestore(ctx);
}

static void gpu_set_clip(vgp_render_backend_t *b, void *ctx,
                          float x, float y, float w, float h)
{
    (void)b;
    nvgScissor(ctx, x, y, w, h);
}

/* ============================================================
 * Vtable
 * ============================================================ */

static const vgp_render_ops_t gpu_ops = {
    .init            = gpu_init,
    .destroy         = gpu_destroy,
    .output_init     = gpu_output_init,
    .output_destroy  = gpu_output_destroy,
    .begin_frame     = gpu_begin_frame,
    .end_frame       = gpu_end_frame,
    .draw_rect       = gpu_draw_rect,
    .draw_rounded_rect = gpu_draw_rounded_rect,
    .draw_circle     = gpu_draw_circle,
    .draw_line       = gpu_draw_line,
    .draw_text       = gpu_draw_text,
    .texture_create  = gpu_texture_create,
    .texture_destroy = gpu_texture_destroy,
    .texture_upload  = gpu_texture_upload,
    .draw_texture    = gpu_draw_texture,
    .draw_shader_rect = gpu_draw_shader_rect,
    .push_state      = gpu_push_state,
    .pop_state       = gpu_pop_state,
    .set_clip        = gpu_set_clip,
    .get_fb_id       = gpu_get_fb_id,
};

vgp_render_backend_t *vgp_gpu_backend_create(void)
{
    vgp_render_backend_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->type = VGP_BACKEND_GPU;
    b->ops = &gpu_ops;
    return b;
}

bool vgp_gpu_backend_available(int drm_fd)
{
    struct gbm_device *gbm = gbm_create_device(drm_fd);
    if (!gbm) return false;

    EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gbm, NULL);
    if (dpy == EGL_NO_DISPLAY) { gbm_device_destroy(gbm); return false; }

    EGLint maj, min;
    if (!eglInitialize(dpy, &maj, &min)) { gbm_device_destroy(gbm); return false; }
    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                           EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE };
    EGLConfig cfg;
    EGLint n;
    bool ok = eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) && n > 0;

    eglTerminate(dpy);
    gbm_device_destroy(gbm);
    return ok;
}

#endif /* VGP_HAS_GPU_BACKEND */