#include "render_backend.h"
#include "drm.h"
#include "vgp/log.h"

#include <plutovg.h>
#include <stdlib.h>
#include <string.h>

#define TAG "cpu-backend"

typedef struct vgp_cpu_output {
    plutovg_surface_t *surfaces[2];
    plutovg_canvas_t  *canvases[2];
    bool               initialized;
} vgp_cpu_output_t;

typedef struct vgp_cpu_state {
    vgp_cpu_output_t    outputs[VGP_MAX_OUTPUTS];
    plutovg_font_face_t *font_face;
} vgp_cpu_state_t;

/* CPU texture: wraps a plutovg_surface_t */
typedef struct vgp_cpu_texture {
    plutovg_surface_t *surface;
} vgp_cpu_texture_t;

/* ============================================================
 * Font loading
 * ============================================================ */

static plutovg_font_face_t *load_font(void)
{
    const char *paths[] = {
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/Hack-Regular.ttf",
        "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Regular.ttf",
        NULL,
    };
    for (const char **p = paths; *p; p++) {
        plutovg_font_face_t *f = plutovg_font_face_load_from_file(*p, 0);
        if (f) {
            VGP_LOG_INFO(TAG, "loaded font: %s", *p);
            return f;
        }
    }
    VGP_LOG_WARN(TAG, "no font found");
    return NULL;
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

static int cpu_init(vgp_render_backend_t *b, int drm_fd)
{
    vgp_cpu_state_t *s = calloc(1, sizeof(*s));
    if (!s) return -1;

    s->font_face = load_font();
    b->priv = s;
    b->drm_fd = drm_fd;
    VGP_LOG_INFO(TAG, "CPU backend initialized");
    return 0;
}

static void cpu_destroy(vgp_render_backend_t *b)
{
    vgp_cpu_state_t *s = b->priv;
    if (!s) return;

    for (int i = 0; i < VGP_MAX_OUTPUTS; i++) {
        for (int buf = 0; buf < 2; buf++) {
            if (s->outputs[i].canvases[buf])
                plutovg_canvas_destroy(s->outputs[i].canvases[buf]);
            if (s->outputs[i].surfaces[buf])
                plutovg_surface_destroy(s->outputs[i].surfaces[buf]);
        }
    }
    if (s->font_face)
        plutovg_font_face_destroy(s->font_face);
    free(s);
    b->priv = NULL;
}

static int cpu_output_init(vgp_render_backend_t *b, int idx,
                            struct vgp_drm_output *output)
{
    vgp_cpu_state_t *s = b->priv;
    if (idx < 0 || idx >= VGP_MAX_OUTPUTS) return -1;

    for (int buf = 0; buf < 2; buf++) {
        vgp_drm_fb_t *fb = &output->fbs[buf];

        if (s->outputs[idx].canvases[buf])
            plutovg_canvas_destroy(s->outputs[idx].canvases[buf]);
        if (s->outputs[idx].surfaces[buf])
            plutovg_surface_destroy(s->outputs[idx].surfaces[buf]);

        s->outputs[idx].surfaces[buf] =
            plutovg_surface_create_for_data(fb->map, (int)fb->width,
                                             (int)fb->height, (int)fb->stride);
        s->outputs[idx].canvases[buf] =
            plutovg_canvas_create(s->outputs[idx].surfaces[buf]);
    }
    s->outputs[idx].initialized = true;
    VGP_LOG_INFO(TAG, "output %d: surfaces created (%ux%u)",
                 idx, output->width, output->height);
    return 0;
}

static void cpu_output_destroy(vgp_render_backend_t *b, int idx)
{
    vgp_cpu_state_t *s = b->priv;
    if (idx < 0 || idx >= VGP_MAX_OUTPUTS) return;

    for (int buf = 0; buf < 2; buf++) {
        if (s->outputs[idx].canvases[buf])
            plutovg_canvas_destroy(s->outputs[idx].canvases[buf]);
        if (s->outputs[idx].surfaces[buf])
            plutovg_surface_destroy(s->outputs[idx].surfaces[buf]);
        s->outputs[idx].canvases[buf] = NULL;
        s->outputs[idx].surfaces[buf] = NULL;
    }
    s->outputs[idx].initialized = false;
}

/* ============================================================
 * Frame lifecycle
 * ============================================================ */

static void *cpu_begin_frame(vgp_render_backend_t *b, int output_idx,
                              struct vgp_drm_output *output)
{
    vgp_cpu_state_t *s = b->priv;
    if (!s->outputs[output_idx].initialized) return NULL;
    int back = 1 - output->front;
    return s->outputs[output_idx].canvases[back];
}

static void cpu_end_frame(vgp_render_backend_t *b, int output_idx,
                           struct vgp_drm_output *output)
{
    (void)b; (void)output_idx; (void)output;
    /* Nothing to do -- plutovg renders directly into the mmap'd buffer */
}

static uint32_t cpu_get_fb_id(vgp_render_backend_t *b, int output_idx,
                               struct vgp_drm_output *output)
{
    (void)b; (void)output_idx;
    int back = 1 - output->front;
    return output->fbs[back].fb_id;
}

/* ============================================================
 * Drawing primitives
 * ============================================================ */

static void cpu_draw_rect(vgp_render_backend_t *b, void *ctx,
                           float x, float y, float w, float h,
                           float r, float g, float ba, float a)
{
    (void)b;
    plutovg_canvas_t *c = ctx;
    plutovg_canvas_set_rgba(c, r, g, ba, a);
    plutovg_canvas_rect(c, x, y, w, h);
    plutovg_canvas_fill(c);
}

static void cpu_draw_rounded_rect(vgp_render_backend_t *b, void *ctx,
                                   float x, float y, float w, float h,
                                   float radius,
                                   float r, float g, float ba, float a)
{
    (void)b;
    plutovg_canvas_t *c = ctx;
    plutovg_canvas_set_rgba(c, r, g, ba, a);
    plutovg_canvas_round_rect(c, x, y, w, h, radius, radius);
    plutovg_canvas_fill(c);
}

static void cpu_draw_circle(vgp_render_backend_t *b, void *ctx,
                             float cx, float cy, float rad,
                             float r, float g, float ba, float a)
{
    (void)b;
    plutovg_canvas_t *c = ctx;
    plutovg_canvas_set_rgba(c, r, g, ba, a);
    plutovg_canvas_circle(c, cx, cy, rad);
    plutovg_canvas_fill(c);
}

static void cpu_draw_line(vgp_render_backend_t *b, void *ctx,
                           float x1, float y1, float x2, float y2,
                           float width,
                           float r, float g, float ba, float a)
{
    (void)b;
    plutovg_canvas_t *c = ctx;
    plutovg_canvas_set_rgba(c, r, g, ba, a);
    plutovg_canvas_set_line_width(c, width);
    plutovg_canvas_move_to(c, x1, y1);
    plutovg_canvas_line_to(c, x2, y2);
    plutovg_canvas_stroke(c);
}

static void cpu_draw_text(vgp_render_backend_t *b, void *ctx,
                           const char *text, int len, float x, float y,
                           float size,
                           float r, float g, float ba, float a)
{
    vgp_cpu_state_t *s = b->priv;
    if (!s->font_face) return;
    plutovg_canvas_t *c = ctx;
    plutovg_canvas_set_rgba(c, r, g, ba, a);
    plutovg_canvas_set_font(c, s->font_face, size);
    plutovg_canvas_fill_text(c, text, len, PLUTOVG_TEXT_ENCODING_UTF8, x, y);
}

/* ============================================================
 * Texture operations
 * ============================================================ */

static vgp_texture_t *cpu_texture_create(vgp_render_backend_t *b,
                                          uint32_t w, uint32_t h)
{
    (void)b;
    vgp_texture_t *tex = calloc(1, sizeof(*tex));
    if (!tex) return NULL;

    vgp_cpu_texture_t *ct = calloc(1, sizeof(*ct));
    if (!ct) { free(tex); return NULL; }

    ct->surface = plutovg_surface_create((int)w, (int)h);
    if (!ct->surface) { free(ct); free(tex); return NULL; }

    tex->width = w;
    tex->height = h;
    tex->priv = ct;
    return tex;
}

static void cpu_texture_destroy(vgp_render_backend_t *b, vgp_texture_t *tex)
{
    (void)b;
    if (!tex) return;
    vgp_cpu_texture_t *ct = tex->priv;
    if (ct) {
        if (ct->surface)
            plutovg_surface_destroy(ct->surface);
        free(ct);
    }
    free(tex);
}

static void cpu_texture_upload(vgp_render_backend_t *b, vgp_texture_t *tex,
                                const uint8_t *data, uint32_t stride)
{
    (void)b;
    vgp_cpu_texture_t *ct = tex->priv;
    uint8_t *dst = plutovg_surface_get_data(ct->surface);
    int dst_stride = plutovg_surface_get_stride(ct->surface);
    for (uint32_t row = 0; row < tex->height; row++)
        memcpy(dst + row * dst_stride, data + row * stride, tex->width * 4);
}

static void cpu_draw_texture(vgp_render_backend_t *b, void *ctx,
                              vgp_texture_t *tex,
                              float dx, float dy, float dw, float dh,
                              float alpha)
{
    (void)b;
    vgp_cpu_texture_t *ct = tex->priv;
    plutovg_canvas_t *c = ctx;

    plutovg_canvas_save(c);
    plutovg_canvas_rect(c, dx, dy, dw, dh);
    plutovg_canvas_clip(c);

    plutovg_matrix_t mat;
    plutovg_matrix_init_translate(&mat, -dx, -dy);
    if (dw != (float)tex->width || dh != (float)tex->height) {
        float sx = (float)tex->width / dw;
        float sy = (float)tex->height / dh;
        plutovg_matrix_scale(&mat, sx, sy);
    }
    plutovg_canvas_set_texture(c, ct->surface, PLUTOVG_TEXTURE_TYPE_PLAIN,
                                alpha, &mat);
    plutovg_canvas_rect(c, dx, dy, dw, dh);
    plutovg_canvas_fill(c);

    plutovg_canvas_restore(c);
}

/* ============================================================
 * Shader (no-op on CPU)
 * ============================================================ */

static void cpu_draw_shader_rect(vgp_render_backend_t *b, void *ctx,
                                  int shader_id,
                                  float x, float y, float w, float h,
                                  float time)
{
    (void)b; (void)ctx; (void)shader_id; (void)x; (void)y;
    (void)w; (void)h; (void)time;
    /* Shaders are GPU-only. CPU backend ignores this. */
}

/* ============================================================
 * State management
 * ============================================================ */

static void cpu_push_state(vgp_render_backend_t *b, void *ctx)
{
    (void)b;
    plutovg_canvas_save(ctx);
}

static void cpu_pop_state(vgp_render_backend_t *b, void *ctx)
{
    (void)b;
    plutovg_canvas_restore(ctx);
}

static void cpu_set_clip(vgp_render_backend_t *b, void *ctx,
                          float x, float y, float w, float h)
{
    (void)b;
    plutovg_canvas_t *c = ctx;
    plutovg_canvas_rect(c, x, y, w, h);
    plutovg_canvas_clip(c);
}

/* ============================================================
 * Vtable
 * ============================================================ */

static const vgp_render_ops_t cpu_ops = {
    .init            = cpu_init,
    .destroy         = cpu_destroy,
    .output_init     = cpu_output_init,
    .output_destroy  = cpu_output_destroy,
    .begin_frame     = cpu_begin_frame,
    .end_frame       = cpu_end_frame,
    .draw_rect       = cpu_draw_rect,
    .draw_rounded_rect = cpu_draw_rounded_rect,
    .draw_circle     = cpu_draw_circle,
    .draw_line       = cpu_draw_line,
    .draw_text       = cpu_draw_text,
    .texture_create  = cpu_texture_create,
    .texture_destroy = cpu_texture_destroy,
    .texture_upload  = cpu_texture_upload,
    .draw_texture    = cpu_draw_texture,
    .draw_shader_rect = cpu_draw_shader_rect,
    .push_state      = cpu_push_state,
    .pop_state       = cpu_pop_state,
    .set_clip        = cpu_set_clip,
    .get_fb_id       = cpu_get_fb_id,
};

vgp_render_backend_t *vgp_cpu_backend_create(void)
{
    vgp_render_backend_t *b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->type = VGP_BACKEND_CPU;
    b->ops = &cpu_ops;
    return b;
}
