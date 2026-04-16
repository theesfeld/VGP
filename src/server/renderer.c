#include "renderer.h"
#include "render_backend.h"
#include "window.h"
#include "notify.h"
#include "animation.h"
#include "lockscreen.h"
#include "menu.h"
#include "calendar.h"
#include "config.h"
#include "panel.h"
#include "vgp-stroke-font.h"
#include "fbo_compose.h"
#include "vgp/log.h"
#include "vgp/protocol.h"

#include <plutovg.h>

#ifdef VGP_HAS_GPU_BACKEND
#include "nanovg.h"
#include "backend_gpu_internal.h"
#include "shader_loader.h"
#endif
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include <errno.h>

#define TAG "renderer"
#define PI 3.14159265f

static void frame_timer_callback(void *data);

/* ============================================================
 * Rendering helpers that use the backend vtable
 * ============================================================ */

static void render_background(vgp_render_backend_t *b, void *ctx,
                               const vgp_theme_t *theme,
                               uint32_t width, uint32_t height,
                               vgp_renderer_t *renderer,
                               float mouse_x, float mouse_y,
                               vgp_compositor_t *comp, int workspace, int32_t out_x)
{
#ifdef VGP_HAS_GPU_BACKEND
    if (renderer->shader_background >= 0 && b->type == VGP_BACKEND_GPU) {
        vgp_gpu_state_t *gs = b->priv;
        vgp_shader_mgr_t *smgr = gs->shader_mgr;
        if (smgr) {
            /* Collect window rects for shadow casting */
            vgp_shader_windows_t wins = {0};
            if (comp) {
                for (int i = 0; i < comp->window_count && wins.count < VGP_SHADER_MAX_WINDOWS; i++) {
                    vgp_window_t *w = comp->z_order[i];
                    if (!w->visible || w->state == VGP_WIN_MINIMIZED) continue;
                    if (w->workspace != workspace) continue;
                    int idx = wins.count * 4;
                    wins.rects[idx + 0] = (float)(w->frame_rect.x - out_x);
                    wins.rects[idx + 1] = (float)w->frame_rect.y;
                    wins.rects[idx + 2] = (float)w->frame_rect.w;
                    wins.rects[idx + 3] = (float)w->frame_rect.h;
                    wins.count++;
                }
            }

            nvgEndFrame(ctx);
            glDisable(GL_BLEND);
            vgp_shader_render(smgr, renderer->shader_background,
                               0, 0, (float)width, (float)height,
                               (float)width, (float)height,
                               theme->background.r, theme->background.g,
                               theme->background.b, theme->background.a,
                               theme->border_active.r, theme->border_active.g,
                               theme->border_active.b, theme->border_active.a,
                               mouse_x, mouse_y, &wins);
            glEnable(GL_BLEND);
            nvgBeginFrame(ctx, (float)width, (float)height, 1.0f);
            return;
        }
    }
#endif
    (void)renderer; (void)mouse_x; (void)mouse_y;
    (void)comp; (void)workspace; (void)out_x;
    const vgp_color_t *c = &theme->background;
    b->ops->draw_rect(b, ctx, 0, 0, (float)width, (float)height,
                       c->r, c->g, c->b, c->a);
}

static void render_decoration(vgp_render_backend_t *b, void *ctx,
                               const vgp_window_t *win,
                               const vgp_theme_t *theme, bool focused)
{
    if (!win->decorated) return;

    const vgp_rect_t *f = &win->frame_rect;
    float x = (float)f->x, y = (float)f->y;
    float w = (float)f->w, h = (float)f->h;
    float th = theme->titlebar_height;
    float cr = theme->corner_radius > 0 ? theme->corner_radius : 10.0f;

    float edge_alpha = focused ? 0.25f : 0.12f;

    /* === Plexiglass pane ===
     * Translucent glass -- the background shows through.
     * Very subtle tint, barely visible surface.
     * FBO blur drawn in separate pass provides the actual blur;
     * this fill is the glass material on top. */
    b->ops->draw_rounded_rect(b, ctx, x, y, w, h, cr,
                               0.08f, 0.08f, 0.10f, focused ? 0.65f : 0.55f);

    /* Glass edge highlight (top-left light source) */
    b->ops->draw_rounded_rect(b, ctx, x, y, w, 1.5f, cr,
                               1.0f, 1.0f, 1.0f, edge_alpha);
    b->ops->draw_rounded_rect(b, ctx, x, y, 1.5f, h, cr,
                               1.0f, 1.0f, 1.0f, edge_alpha * 0.5f);

    /* Glass edge shadow (bottom-right) */
    b->ops->draw_rounded_rect(b, ctx, x, y + h - 1.5f, w, 1.5f, cr,
                               0.0f, 0.0f, 0.0f, edge_alpha);
    b->ops->draw_rounded_rect(b, ctx, x + w - 1.5f, y, 1.5f, h, cr,
                               0.0f, 0.0f, 0.0f, edge_alpha * 0.5f);

    /* Content area: very subtle dark tint for readability */
    const vgp_color_t *cb = &theme->content_bg;
    b->ops->draw_rounded_rect(b, ctx, x + 2, y + th, w - 4, h - th - 2, cr > 2 ? cr - 2 : 1,
                               cb->r, cb->g, cb->b, focused ? 0.85f : 0.75f);

    /* === Etched title text ===
     * "Etched into glass" = dark shadow below, bright text on top.
     * Like a manufacturer mark pressed into the surface. */
    if (win->title[0]) {
        float text_x = x + 12.0f;
        float text_y = y + th / 2.0f + theme->title_font_size / 3.0f;
        float fs = theme->title_font_size;

        /* Shadow (etched groove -- darker, offset down-right) */
        b->ops->draw_text(b, ctx, win->title, -1, text_x + 1, text_y + 1, fs,
                           0.0f, 0.0f, 0.0f, focused ? 0.5f : 0.3f);
        /* Highlight (etched ridge -- brighter, offset up-left) */
        b->ops->draw_text(b, ctx, win->title, -1, text_x - 0.5f, text_y - 0.5f, fs,
                           1.0f, 1.0f, 1.0f, focused ? 0.15f : 0.08f);
        /* Main text */
        const vgp_color_t *tc = focused ? &theme->title_text_active
                                        : &theme->title_text_inactive;
        b->ops->draw_text(b, ctx, win->title, -1, text_x, text_y, fs,
                           tc->r, tc->g, tc->b, focused ? 0.7f : 0.4f);
    }

    /* === Window control dots (small, subtle, etched into glass) === */
    float btn_r = 4.0f;
    float btn_spacing = 10.0f;
    float btn_cy = y + th / 2.0f;

    /* Close */
    float close_cx = x + w - 16 - btn_r;
    b->ops->draw_circle(b, ctx, close_cx + 0.5f, btn_cy + 0.5f, btn_r, 0, 0, 0, 0.3f);
    b->ops->draw_circle(b, ctx, close_cx, btn_cy, btn_r,
                         theme->close_btn.r, theme->close_btn.g,
                         theme->close_btn.b, focused ? 0.8f : 0.4f);

    /* Maximize */
    float max_cx = close_cx - btn_r * 2 - btn_spacing;
    b->ops->draw_circle(b, ctx, max_cx + 0.5f, btn_cy + 0.5f, btn_r, 0, 0, 0, 0.3f);
    b->ops->draw_circle(b, ctx, max_cx, btn_cy, btn_r,
                         theme->maximize_btn.r, theme->maximize_btn.g,
                         theme->maximize_btn.b, focused ? 0.8f : 0.4f);

    /* Minimize */
    float min_cx = max_cx - btn_r * 2 - btn_spacing;
    b->ops->draw_circle(b, ctx, min_cx + 0.5f, btn_cy + 0.5f, btn_r, 0, 0, 0, 0.3f);
    b->ops->draw_circle(b, ctx, min_cx, btn_cy, btn_r,
                         theme->minimize_btn.r, theme->minimize_btn.g,
                         theme->minimize_btn.b, focused ? 0.8f : 0.4f);
}

/* Render a cell grid (vector terminal) directly with the backend */
static void render_cellgrid(vgp_render_backend_t *b, void *ctx,
                             vgp_window_t *win, const vgp_rect_t *content)
{
    vgp_cell_t *cells = win->cellgrid;
    int rows = win->grid_rows;
    int cols = win->grid_cols;
    float cx = (float)content->x;
    float cy = (float)content->y;
    float cw = (float)content->w;
    float ch = (float)content->h;

    /* Compute cell dimensions from content area */
    float cell_w = cw / (float)cols;
    float cell_h = ch / (float)rows;
    float font_size = (win->font_size_override > 0) ?
        win->font_size_override : cell_h * 0.75f;
    float baseline_offset = cell_h * 0.72f;

    b->ops->push_state(b, ctx);
    b->ops->set_clip(b, ctx, cx, cy, cw, ch);

    for (int row = 0; row < rows; row++) {
        float y = cy + (float)row * cell_h;

        for (int col = 0; col < cols; ) {
            vgp_cell_t *cell = &cells[row * cols + col];
            int span = cell->width > 0 ? cell->width : 1;

            float x = cx + (float)col * cell_w;
            float w = (float)span * cell_w;

            uint8_t fg_r = cell->fg_r, fg_g = cell->fg_g, fg_b = cell->fg_b;
            uint8_t bg_r = cell->bg_r, bg_g = cell->bg_g, bg_b = cell->bg_b;

            if (cell->attrs & VGP_CELL_REVERSE) {
                uint8_t tr = fg_r, tg = fg_g, tb = fg_b;
                fg_r = bg_r; fg_g = bg_g; fg_b = bg_b;
                bg_r = tr; bg_g = tg; bg_b = tb;
            }

            /* Background (skip if it matches default bg to avoid overdraw) */
            if (bg_r != 30 || bg_g != 30 || bg_b != 30) { /* approx 0.12*255 */
                b->ops->draw_rect(b, ctx, x, y, w, cell_h,
                                   bg_r / 255.0f, bg_g / 255.0f, bg_b / 255.0f, 1.0f);
            }

            /* Text */
            if (cell->codepoint > 32) {
                char utf8[5] = {0};
                uint32_t cp = cell->codepoint;
                if (cp < 0x80) {
                    utf8[0] = (char)cp;
                } else if (cp < 0x800) {
                    utf8[0] = (char)(0xC0 | (cp >> 6));
                    utf8[1] = (char)(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    utf8[0] = (char)(0xE0 | (cp >> 12));
                    utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    utf8[2] = (char)(0x80 | (cp & 0x3F));
                } else {
                    utf8[0] = (char)(0xF0 | (cp >> 18));
                    utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                    utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    utf8[3] = (char)(0x80 | (cp & 0x3F));
                }

                b->ops->draw_text(b, ctx, utf8, -1,
                                   x, y + baseline_offset, font_size,
                                   fg_r / 255.0f, fg_g / 255.0f, fg_b / 255.0f, 1.0f);
            }

            /* Underline */
            if (cell->attrs & VGP_CELL_UNDERLINE) {
                float uy = y + baseline_offset + 2.0f;
                b->ops->draw_rect(b, ctx, x, uy, w, 1.0f,
                                   fg_r / 255.0f, fg_g / 255.0f, fg_b / 255.0f, 1.0f);
            }

            /* Strikethrough */
            if (cell->attrs & VGP_CELL_STRIKE) {
                float sy = y + cell_h / 2.0f;
                b->ops->draw_rect(b, ctx, x, sy, w, 1.0f,
                                   fg_r / 255.0f, fg_g / 255.0f, fg_b / 255.0f, 1.0f);
            }

            col += span;
        }
    }

    /* Cursor */
    if (win->cursor_visible && win->cursor_row < rows && win->cursor_col < cols) {
        float cur_x = cx + (float)win->cursor_col * cell_w;
        float cur_y = cy + (float)win->cursor_row * cell_h;

        switch (win->cursor_shape) {
        case 1: /* block */
            b->ops->draw_rect(b, ctx, cur_x, cur_y, cell_w, cell_h,
                               0.85f, 0.85f, 0.85f, 0.7f);
            break;
        case 2: /* underline */
            b->ops->draw_rect(b, ctx, cur_x, cur_y + cell_h - 2, cell_w, 2.0f,
                               0.85f, 0.85f, 0.85f, 0.9f);
            break;
        case 3: /* bar */
            b->ops->draw_rect(b, ctx, cur_x, cur_y, 2.0f, cell_h,
                               0.85f, 0.85f, 0.85f, 0.9f);
            break;
        }
    }

    b->ops->pop_state(b, ctx);
}

/* Render a draw command stream (graphical UI protocol) */
static void render_drawcmds(vgp_render_backend_t *b, void *ctx,
                              vgp_window_t *win, const vgp_rect_t *content)
{
    const uint8_t *buf = win->draw_cmds;
    size_t len = win->draw_cmds_len;
    float ox = (float)content->x;
    float oy = (float)content->y;
    size_t off = 0;

    b->ops->push_state(b, ctx);
    b->ops->set_clip(b, ctx, ox, oy, (float)content->w, (float)content->h);

    while (off < len) {
        uint8_t op = buf[off++];
        const float *f = (const float *)(buf + off);

        switch (op) {
        case VGP_DCMD_CLEAR:
            b->ops->draw_rect(b, ctx, ox, oy, (float)content->w, (float)content->h,
                               f[0], f[1], f[2], f[3]);
            off += 16; break;

        case VGP_DCMD_RECT:
            b->ops->draw_rect(b, ctx, f[0]+ox, f[1]+oy, f[2], f[3],
                               f[4], f[5], f[6], f[7]);
            off += 32; break;

        case VGP_DCMD_ROUNDED_RECT:
            b->ops->draw_rounded_rect(b, ctx, f[0]+ox, f[1]+oy, f[2], f[3], f[4],
                                       f[5], f[6], f[7], f[8]);
            off += 36; break;

        case VGP_DCMD_CIRCLE:
            b->ops->draw_circle(b, ctx, f[0]+ox, f[1]+oy, f[2],
                                 f[3], f[4], f[5], f[6]);
            off += 28; break;

        case VGP_DCMD_LINE:
            b->ops->draw_line(b, ctx, f[0]+ox, f[1]+oy, f[2]+ox, f[3]+oy, f[4],
                               f[5], f[6], f[7], f[8]);
            off += 36; break;

        case VGP_DCMD_TEXT:
        case VGP_DCMD_TEXT_BOLD: {
            float tx = f[0]+ox, ty = f[1]+oy, sz = f[2];
            float cr = f[3], cg = f[4], cb = f[5], ca = f[6];
            uint16_t tlen;
            memcpy(&tlen, buf + off + 28, 2);
            const char *text = (const char *)(buf + off + 30);
            float lw = (op == VGP_DCMD_TEXT_BOLD) ? 2.0f : 1.2f;
            /* Scale from grid coordinates to pixel size */
            float scale = sz / (float)STROKE_GRID_H;
            float advance = ((float)STROKE_GRID_W + 1.0f) * scale;
            float cursor_x = tx;
            /* ty is baseline -- glyphs render from top, so offset up by sz */
            float glyph_y = ty - sz;
            for (uint16_t ci = 0; ci < tlen && text[ci]; ci++) {
                const stroke_glyph_t *g = stroke_font_glyph((int)(unsigned char)text[ci]);
                for (int si = 0; si < STROKE_MAX_SEG; si++) {
                    const stroke_seg_t *s = &g->segs[si];
                    if (s->x1 == STROKE_END) break;
                    float x1 = cursor_x + (float)s->x1 * scale;
                    float y1 = glyph_y + (float)s->y1 * scale;
                    float x2 = cursor_x + (float)s->x2 * scale;
                    float y2 = glyph_y + (float)s->y2 * scale;
                    b->ops->draw_line(b, ctx, x1, y1, x2, y2, lw, cr, cg, cb, ca);
                }
                cursor_x += advance;
            }
            off += 30 + tlen;
            break;
        }

        case VGP_DCMD_PUSH_STATE:
            b->ops->push_state(b, ctx);
            break;

        case VGP_DCMD_POP_STATE:
            b->ops->pop_state(b, ctx);
            break;

        case VGP_DCMD_SET_CLIP:
            b->ops->set_clip(b, ctx, f[0]+ox, f[1]+oy, f[2], f[3]);
            off += 16; break;

        case VGP_DCMD_RECT_OUTLINE:
            /* Draw 4 edges as thin rects (works on both backends) */
            {
                float x=f[0]+ox, y=f[1]+oy, w=f[2], h=f[3], lw=f[4];
                float cr=f[5], cg=f[6], cb=f[7], ca=f[8];
                b->ops->draw_rect(b, ctx, x, y, w, lw, cr, cg, cb, ca);
                b->ops->draw_rect(b, ctx, x, y+h-lw, w, lw, cr, cg, cb, ca);
                b->ops->draw_rect(b, ctx, x, y, lw, h, cr, cg, cb, ca);
                b->ops->draw_rect(b, ctx, x+w-lw, y, lw, h, cr, cg, cb, ca);
            }
            off += 36; break;

        case VGP_DCMD_RRECT_OUTLINE:
#ifdef VGP_HAS_GPU_BACKEND
            if (b->type == VGP_BACKEND_GPU) {
                float x=f[0]+ox, y=f[1]+oy, w=f[2], h=f[3], rad=f[4], lw=f[5];
                float cr=f[6], cg=f[7], cb=f[8], ca=f[9];
                NVGcontext *vg = ctx;
                nvgBeginPath(vg);
                nvgRoundedRect(vg, x, y, w, h, rad);
                nvgStrokeColor(vg, nvgRGBAf(cr, cg, cb, ca));
                nvgStrokeWidth(vg, lw);
                nvgStroke(vg);
            } else
#endif
            {
                float x=f[0]+ox, y=f[1]+oy, w=f[2], h=f[3], lw=f[5];
                float cr=f[6], cg=f[7], cb=f[8], ca=f[9];
                b->ops->draw_rect(b, ctx, x, y, w, lw, cr, cg, cb, ca);
                b->ops->draw_rect(b, ctx, x, y+h-lw, w, lw, cr, cg, cb, ca);
                b->ops->draw_rect(b, ctx, x, y, lw, h, cr, cg, cb, ca);
                b->ops->draw_rect(b, ctx, x+w-lw, y, lw, h, cr, cg, cb, ca);
            }
            off += 40; break;

        case VGP_DCMD_GRADIENT_RECT:
#ifdef VGP_HAS_GPU_BACKEND
            if (b->type == VGP_BACKEND_GPU) {
                float x=f[0]+ox, y=f[1]+oy, w=f[2], h=f[3];
                float r1=f[4],g1=f[5],b1=f[6],a1=f[7];
                float r2=f[8],g2=f[9],b2=f[10],a2=f[11];
                NVGcontext *vg = ctx;
                NVGpaint paint = nvgLinearGradient(vg, x, y, x, y+h,
                                                     nvgRGBAf(r1,g1,b1,a1),
                                                     nvgRGBAf(r2,g2,b2,a2));
                nvgBeginPath(vg);
                nvgRect(vg, x, y, w, h);
                nvgFillPaint(vg, paint);
                nvgFill(vg);
            } else
#endif
            {
                float x=f[0]+ox, y=f[1]+oy, w=f[2], h=f[3];
                float r1=f[4],g1=f[5],b1=f[6],a1=f[7];
                float r2=f[8],g2=f[9],b2=f[10],a2=f[11];
                int bands = 16;
                float bh = h / (float)bands;
                for (int i = 0; i < bands; i++) {
                    float t = (float)i / (float)(bands - 1);
                    b->ops->draw_rect(b, ctx, x, y + (float)i * bh, w, bh + 1,
                                       r1+(r2-r1)*t, g1+(g2-g1)*t,
                                       b1+(b2-b1)*t, a1+(a2-a1)*t);
                }
            }
            off += 48; break;

        case VGP_DCMD_TRANSFORM:
#ifdef VGP_HAS_GPU_BACKEND
            if (b->type == VGP_BACKEND_GPU) {
                NVGcontext *vg = ctx;
                nvgTransform(vg, f[0], f[1], f[2], f[3], f[4], f[5]);
            }
#endif
            off += 24; break;

        default:
            /* Unknown opcode -- bail to prevent reading garbage */
            off = len;
            break;
        }
    }

    b->ops->pop_state(b, ctx);
}

static void render_window_content(vgp_render_backend_t *b, void *ctx,
                                   vgp_window_t *win, int32_t offset_x)
{
    vgp_rect_t content = win->content_rect;
    content.x -= offset_x;

    /* Draw commands (graphical UI) -- highest priority */
    if (win->has_drawcmds && win->draw_cmds) {
        render_drawcmds(b, ctx, win, &content);
        return;
    }

    /* Vector cell grid -- render text directly with GPU */
    if (win->has_cellgrid && win->cellgrid) {
        render_cellgrid(b, ctx, win, &content);
        return;
    }

    /* Legacy pixel surface fallback */
    if (!win->client_surface) return;

    const vgp_rect_t *c = &content;

    if (b->type == VGP_BACKEND_CPU) {
        plutovg_canvas_t *canvas = ctx;
        plutovg_canvas_save(canvas);
        plutovg_canvas_rect(canvas, (float)c->x, (float)c->y,
                             (float)c->w, (float)c->h);
        plutovg_canvas_clip(canvas);
        plutovg_matrix_t mat;
        plutovg_matrix_init_translate(&mat, -(float)c->x, -(float)c->y);
        plutovg_canvas_set_texture(canvas, win->client_surface,
                                    PLUTOVG_TEXTURE_TYPE_PLAIN, 1.0f, &mat);
        plutovg_canvas_rect(canvas, (float)c->x, (float)c->y,
                             (float)c->w, (float)c->h);
        plutovg_canvas_fill(canvas);
        plutovg_canvas_restore(canvas);
    }
#ifdef VGP_HAS_GPU_BACKEND
    else {
        int iw = (int)win->client_width;
        int ih = (int)win->client_height;
        uint8_t *src = plutovg_surface_get_data(win->client_surface);
        int src_stride = plutovg_surface_get_stride(win->client_surface);
        NVGcontext *vg = ctx;

        /* BGRA -> RGBA swizzle into temp buffer */
        uint8_t *rgba = malloc((size_t)iw * (size_t)ih * 4);
        if (!rgba) return;
        for (int row = 0; row < ih; row++) {
            uint32_t *sp = (uint32_t *)(src + row * src_stride);
            uint32_t *dp = (uint32_t *)(rgba + row * iw * 4);
            for (int px = 0; px < iw; px++) {
                uint32_t p = sp[px];
                dp[px] = (p & 0xFF00FF00u)
                       | ((p >> 16) & 0xFFu)
                       | ((p & 0xFFu) << 16);
            }
        }

        /* Persistent NanoVG image -- create once, update each frame */
        if (win->nvg_image <= 0 ||
            win->nvg_image_w != (uint32_t)iw ||
            win->nvg_image_h != (uint32_t)ih) {
            if (win->nvg_image > 0)
                nvgDeleteImage(vg, win->nvg_image);
            win->nvg_image = nvgCreateImageRGBA(vg, iw, ih,
                                                 NVG_IMAGE_PREMULTIPLIED, rgba);
            win->nvg_image_w = (uint32_t)iw;
            win->nvg_image_h = (uint32_t)ih;
        } else {
            nvgUpdateImage(vg, win->nvg_image, rgba);
        }
        free(rgba);

        if (win->nvg_image > 0) {
            NVGpaint paint = nvgImagePattern(vg, (float)c->x, (float)c->y,
                                              (float)c->w, (float)c->h,
                                              0, win->nvg_image, 1.0f);
            nvgBeginPath(vg);
            nvgRect(vg, (float)c->x, (float)c->y,
                     (float)c->w, (float)c->h);
            nvgFillPaint(vg, paint);
            nvgFill(vg);
        }
    }
#endif
}

static void render_cursor_cpu(plutovg_canvas_t *c, float x, float y)
{
    plutovg_canvas_save(c);
    plutovg_canvas_move_to(c, x, y);
    plutovg_canvas_line_to(c, x, y + 16);
    plutovg_canvas_line_to(c, x + 4, y + 12);
    plutovg_canvas_line_to(c, x + 7, y + 18);
    plutovg_canvas_line_to(c, x + 9, y + 16);
    plutovg_canvas_line_to(c, x + 6, y + 11);
    plutovg_canvas_line_to(c, x + 12, y + 11);
    plutovg_canvas_close_path(c);
    plutovg_canvas_set_rgb(c, 0, 0, 0);
    plutovg_canvas_set_line_width(c, 1.5f);
    plutovg_canvas_stroke_preserve(c);
    plutovg_canvas_set_rgb(c, 1, 1, 1);
    plutovg_canvas_fill(c);
    plutovg_canvas_restore(c);
}

static void render_cursor(vgp_render_backend_t *b, void *ctx,
                           vgp_cursor_t *cursor)
{
    if (!cursor->visible) return;

    float x = cursor->x, y = cursor->y;

    if (b->type == VGP_BACKEND_CPU) {
        render_cursor_cpu(ctx, x, y);
    } else {
        /* GPU: draw cursor shape based on context */
        b->ops->push_state(b, ctx);

        switch (cursor->shape) {
        default:
        case VGP_CURSOR_ARROW:
            /* Arrow pointer via line segments + fill */
            b->ops->draw_line(b, ctx, x, y, x, y + 16, 1.5f, 0, 0, 0, 1);
            b->ops->draw_line(b, ctx, x, y + 16, x + 4, y + 12, 1.5f, 0, 0, 0, 1);
            b->ops->draw_line(b, ctx, x + 4, y + 12, x + 7, y + 18, 1.5f, 0, 0, 0, 1);
            b->ops->draw_line(b, ctx, x + 7, y + 18, x + 9, y + 16, 1.5f, 0, 0, 0, 1);
            b->ops->draw_line(b, ctx, x + 9, y + 16, x + 6, y + 11, 1.5f, 0, 0, 0, 1);
            b->ops->draw_line(b, ctx, x + 6, y + 11, x + 12, y + 11, 1.5f, 0, 0, 0, 1);
            b->ops->draw_line(b, ctx, x + 12, y + 11, x, y, 1.5f, 0, 0, 0, 1);
            b->ops->draw_rect(b, ctx, x + 1, y + 1, 6, 14, 1, 1, 1, 0.9f);
            break;

        case VGP_CURSOR_RESIZE_N:
        case VGP_CURSOR_RESIZE_S:
            /* Vertical resize: up/down arrows */
            b->ops->draw_rect(b, ctx, x - 1, y - 8, 2, 16, 1, 1, 1, 0.9f);
            b->ops->draw_line(b, ctx, x - 4, y - 4, x, y - 8, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 4, y - 4, x, y - 8, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x - 4, y + 4, x, y + 8, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 4, y + 4, x, y + 8, 2, 1, 1, 1, 1);
            break;

        case VGP_CURSOR_RESIZE_E:
        case VGP_CURSOR_RESIZE_W:
            /* Horizontal resize: left/right arrows */
            b->ops->draw_rect(b, ctx, x - 8, y - 1, 16, 2, 1, 1, 1, 0.9f);
            b->ops->draw_line(b, ctx, x - 4, y - 4, x - 8, y, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x - 4, y + 4, x - 8, y, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 4, y - 4, x + 8, y, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 4, y + 4, x + 8, y, 2, 1, 1, 1, 1);
            break;

        case VGP_CURSOR_RESIZE_NE:
        case VGP_CURSOR_RESIZE_SW:
            /* Diagonal NE-SW */
            b->ops->draw_line(b, ctx, x - 6, y + 6, x + 6, y - 6, 2, 1, 1, 1, 0.9f);
            b->ops->draw_line(b, ctx, x + 2, y - 6, x + 6, y - 6, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 6, y - 2, x + 6, y - 6, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x - 2, y + 6, x - 6, y + 6, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x - 6, y + 2, x - 6, y + 6, 2, 1, 1, 1, 1);
            break;

        case VGP_CURSOR_RESIZE_NW:
        case VGP_CURSOR_RESIZE_SE:
            /* Diagonal NW-SE */
            b->ops->draw_line(b, ctx, x - 6, y - 6, x + 6, y + 6, 2, 1, 1, 1, 0.9f);
            b->ops->draw_line(b, ctx, x - 2, y - 6, x - 6, y - 6, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x - 6, y - 2, x - 6, y - 6, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 2, y + 6, x + 6, y + 6, 2, 1, 1, 1, 1);
            b->ops->draw_line(b, ctx, x + 6, y + 2, x + 6, y + 6, 2, 1, 1, 1, 1);
            break;

        case VGP_CURSOR_MOVE:
            /* Move: four-direction arrows */
            b->ops->draw_rect(b, ctx, x - 8, y - 1, 16, 2, 1, 1, 1, 0.9f);
            b->ops->draw_rect(b, ctx, x - 1, y - 8, 2, 16, 1, 1, 1, 0.9f);
            break;

        case VGP_CURSOR_TEXT:
            /* I-beam */
            b->ops->draw_rect(b, ctx, x - 1, y - 8, 2, 16, 1, 1, 1, 0.9f);
            b->ops->draw_rect(b, ctx, x - 4, y - 8, 8, 2, 1, 1, 1, 0.7f);
            b->ops->draw_rect(b, ctx, x - 4, y + 6, 8, 2, 1, 1, 1, 0.7f);
            break;

        case VGP_CURSOR_HAND:
            /* Hand/pointer (simplified) */
            b->ops->draw_circle(b, ctx, x + 2, y - 2, 4, 1, 1, 1, 0.9f);
            b->ops->draw_rect(b, ctx, x, y + 2, 4, 10, 1, 1, 1, 0.9f);
            break;
        }

        b->ops->pop_state(b, ctx);
    }
}

/* Panel rendering is in panel.c */

/* ============================================================
 * Renderer lifecycle
 * ============================================================ */

int vgp_renderer_init(vgp_renderer_t *renderer, vgp_drm_backend_t *drm,
                       vgp_event_loop_t *loop, struct vgp_server *server)
{
    memset(renderer, 0, sizeof(*renderer));

    /* Try GPU first, fall back to CPU. Set VGP_CPU=1 to force CPU. */
    const char *force_cpu = getenv("VGP_CPU");
    bool try_gpu = !(force_cpu && force_cpu[0] == '1');

#ifdef VGP_HAS_GPU_BACKEND
    if (try_gpu && vgp_gpu_backend_available(drm->drm_fd)) {
        renderer->backend = vgp_gpu_backend_create();
        if (renderer->backend) {
            if (renderer->backend->ops->init(renderer->backend, drm->drm_fd) < 0) {
                VGP_LOG_WARN(TAG, "GPU backend init failed, falling back to CPU");
                free(renderer->backend);
                renderer->backend = NULL;
            } else {
                VGP_LOG_INFO(TAG, "using GPU backend");
            }
        }
    } else if (try_gpu) {
        VGP_LOG_INFO(TAG, "GPU not available, using CPU backend");
    }
#else
    (void)try_gpu;
#endif

    if (!renderer->backend) {
        renderer->backend = vgp_cpu_backend_create();
        if (!renderer->backend || renderer->backend->ops->init(renderer->backend, drm->drm_fd) < 0) {
            VGP_LOG_ERROR(TAG, "CPU backend init failed");
            return -1;
        }
        VGP_LOG_INFO(TAG, "using CPU backend");
    }

    /* Init output surfaces */
    for (int i = 0; i < drm->output_count; i++) {
        if (renderer->backend->ops->output_init(renderer->backend, i, &drm->outputs[i]) < 0)
            VGP_LOG_WARN(TAG, "output %d init failed", i);
    }

    /* Create frame timer */
    if (vgp_timer_create(&renderer->frame_timer, loop,
                          frame_timer_callback, server) < 0) {
        VGP_LOG_ERROR(TAG, "failed to create frame timer");
        return -1;
    }

    renderer->dirty = true;
    renderer->shader_background = -1;
    renderer->shader_titlebar = -1;
    renderer->shader_panel = -1;
    renderer->shader_overlay = -1;

    /* Load shader effects from theme */
#ifdef VGP_HAS_GPU_BACKEND
    if (renderer->backend->type == VGP_BACKEND_GPU) {
        vgp_shader_mgr_t *smgr = calloc(1, sizeof(vgp_shader_mgr_t));
        if (smgr && vgp_shader_mgr_init(smgr) == 0) {
            vgp_gpu_state_t *gs = renderer->backend->priv;
            gs->shader_mgr = smgr;

            /* Load background shader from theme's resolved path */
            /* server is passed as the frame timer's user_data -- but we
             * don't have it here yet. Use the theme's background_shader
             * field which was resolved during config loading. */
            /* For now, search known paths including theme dir */
            const char *home = getenv("HOME");
            char path_buf[512];

            /* Background shader: theme-resolved path first */
            const char *bg_paths[4] = { NULL, NULL, NULL, NULL };
            int bg_count = 0;
            /* We'll get the actual theme path from server after init.
             * For now, try common locations. */
            if (home) {
                snprintf(path_buf, sizeof(path_buf),
                         "%s/.config/vgp/shaders/background.frag", home);
                bg_paths[bg_count++] = strdup(path_buf);
            }
            bg_paths[bg_count++] = "themes/shaders/background.frag";

            for (int i = 0; i < bg_count && bg_paths[i]; i++) {
                renderer->shader_background = vgp_shader_load(smgr, bg_paths[i]);
                if (renderer->shader_background >= 0) break;
            }
            /* Free only dynamically allocated paths (index 0) */
            if (bg_count > 0 && bg_paths[0])
                free((void*)bg_paths[0]);

            /* Panel shader */
            if (home) {
                snprintf(path_buf, sizeof(path_buf),
                         "%s/.config/vgp/shaders/panel.frag", home);
                renderer->shader_panel = vgp_shader_load(smgr, path_buf);
            }

            /* Overlay shader slot reserved for future FBO post-process */
        }
    }
#endif

    /* Initialize FBO compositing pipeline for glass effects */
#ifdef VGP_HAS_GPU_BACKEND
    if (renderer->backend->type == VGP_BACKEND_GPU) {
        vgp_gpu_state_t *gs = renderer->backend->priv;
        if (drm->output_count > 0)
            vgp_fbo_init(gs, drm->outputs[0].width, drm->outputs[0].height);
    }
#endif

    VGP_LOG_INFO(TAG, "renderer initialized (bg=%d, panel=%d)",
                 renderer->shader_background, renderer->shader_panel);
    return 0;
}

void vgp_renderer_destroy(vgp_renderer_t *renderer, vgp_event_loop_t *loop)
{
    vgp_timer_destroy(&renderer->frame_timer, loop);

    if (renderer->backend) {
        renderer->backend->ops->destroy(renderer->backend);
        free(renderer->backend);
        renderer->backend = NULL;
    }
}

void vgp_renderer_schedule_frame(vgp_renderer_t *renderer)
{
    renderer->dirty = true;
    if (renderer->frame_scheduled)
        return;
    vgp_timer_arm_oneshot(&renderer->frame_timer, VGP_MS_TO_NS(2));
    renderer->frame_scheduled = true;
}

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
                                 const vgp_config_panel_t *panel_cfg)
{
    if (output->page_flip_pending)
        return;

    vgp_render_backend_t *b = renderer->backend;

    void *ctx = b->ops->begin_frame(b, output_idx, output);
    if (!ctx) return;

    /* Get the workspace assigned to this output */
    int workspace = 0;
    int32_t out_x = 0;
    if (output_idx < comp->output_count) {
        workspace = comp->outputs[output_idx].workspace;
        out_x = comp->outputs[output_idx].x;
    }

    /* Layer 0: Desktop background (shader or solid color) */
    float local_mouse_x = comp->cursor.x - (float)out_x;
    float local_mouse_y = comp->cursor.y;
#ifdef VGP_HAS_GPU_BACKEND
    if (b->type == VGP_BACKEND_GPU) {
        vgp_gpu_state_t *gs = b->priv;
        vgp_shader_mgr_t *smgr = gs->shader_mgr;
        if (smgr) vgp_shader_mgr_tick(smgr, 0.016f);
    }
#endif
    render_background(b, ctx, theme, output->width, output->height,
                       renderer, local_mouse_x, local_mouse_y,
                       comp, workspace, out_x);

    /* Workspace slide animation offset */
    float ws_slide_offset = 0.0f;
    if (anims) {
        vgp_animation_t *ws_anim = vgp_anim_find_ws_slide(anims,
                                                            (uint32_t)output_idx);
        if (ws_anim)
            ws_slide_offset = vgp_anim_ws_slide_offset(ws_anim,
                                                         (float)output->width);
    }

    /* For GPU backend, we need to translate window coordinates
     * relative to this output's position in the global layout */
    bool need_translate = (out_x != 0);
    if (need_translate) {
        b->ops->push_state(b, ctx);
        /* Shift everything left by the output's x offset so windows
         * on this output appear at the correct position */
    }

    /* === GLASS COMPOSITING PASS ===
     * End NanoVG (flushes background to screen), capture to FBO,
     * then draw frosted glass blur quads for each window.
     * Then restart NanoVG for window content + decorations. */
#ifdef VGP_HAS_GPU_BACKEND
    if (b->type == VGP_BACKEND_GPU) {
        vgp_gpu_state_t *gs = b->priv;
        if (gs->fbo_initialized) {
            /* Flush background render to GL */
            nvgEndFrame(ctx);

            /* Capture background to FBO texture */
            vgp_fbo_resize(gs, output->width, output->height);
            vgp_fbo_capture(gs, output->width, output->height);

            /* Draw glass blur for the panel */
            {
                float bar_h = (panel_cfg->height > 0) ? (float)panel_cfg->height : theme->statusbar_height;
                bool ptop = (strcmp(panel_cfg->position, "top") == 0);
                float panel_y = ptop ? 0 : (float)output->height - bar_h;
                vgp_fbo_draw_blur_rect(gs, 0, panel_y, (float)output->width, bar_h, 0,
                                         4.0f, 0.05f, 0.05f, 0.07f, 0.04f,
                                         output->width, output->height);
            }

            /* Draw glass blur for each visible decorated window */
            for (int gi = 0; gi < comp->window_count; gi++) {
                vgp_window_t *gw = comp->z_order[gi];
                if (!gw->visible || gw->state == VGP_WIN_MINIMIZED) continue;
                if (gw->workspace != workspace) continue;
                if (!gw->decorated) continue;

                float gx = (float)(gw->frame_rect.x - out_x);
                float gy = (float)gw->frame_rect.y;
                float gwidth = (float)gw->frame_rect.w;
                float gheight = (float)gw->frame_rect.h;
                float gcr = theme->corner_radius > 0 ? theme->corner_radius : 10.0f;

                vgp_fbo_draw_blur_rect(gs, gx, gy, gwidth, gheight, gcr,
                                         5.0f,  /* blur radius */
                                         0.08f, 0.08f, 0.10f, 0.06f, /* near-clear tint */
                                         output->width, output->height);
            }

            /* Restart NanoVG for content rendering */
            nvgBeginFrame(ctx, (float)output->width, (float)output->height, 1.0f);
        }
    }
#endif

    /* Layer 1: Windows on this workspace */
    for (int i = 0; i < comp->window_count; i++) {
        vgp_window_t *win = comp->z_order[i];
        if (!win->visible || win->state == VGP_WIN_MINIMIZED)
            continue;
        if (win->workspace != workspace)
            continue;

        /* In expose mode, override window positions with tiled layout */
        if (comp->expose_active && win->id < VGP_MAX_WINDOWS &&
            comp->expose_rects[win->id].w > 0) {
            vgp_window_t tmp_exp = *win;
            vgp_rect_t er = comp->expose_rects[win->id];
            er.x -= out_x;
            tmp_exp.frame_rect = er;
            tmp_exp.content_rect = vgp_window_content_rect(&er, theme);

            /* Subtle shadow */
            b->ops->draw_rounded_rect(b, ctx,
                (float)er.x + 4, (float)er.y + 4,
                (float)er.w, (float)er.h,
                theme->corner_radius, 0, 0, 0, 0.2f);

            render_decoration(b, ctx, &tmp_exp, theme, win == comp->focused);
            render_window_content(b, ctx, win, out_x);
            continue;
        }

        /* Translate window coordinates to output-local space + slide offset */
        vgp_window_t tmp = *win;
        tmp.frame_rect.x -= out_x;
        tmp.content_rect.x -= out_x;
        if (ws_slide_offset != 0.0f) {
            tmp.frame_rect.x += (int32_t)ws_slide_offset;
            tmp.content_rect.x += (int32_t)ws_slide_offset;
        }

        /* Check for active animation on this window */
        float win_opacity = (win == comp->focused) ?
            theme->window_opacity : theme->inactive_opacity;
        if (win_opacity <= 0.0f) win_opacity = 1.0f; /* sanity */

        vgp_animation_t *anim = anims ? vgp_anim_find(anims, win->id) : NULL;
        if (anim) {
            win_opacity *= vgp_anim_opacity(anim);
            if (anim->type == VGP_ANIM_WINDOW_OPEN || anim->type == VGP_ANIM_WINDOW_CLOSE) {
                /* Scale animation for open/close */
                float scale = vgp_anim_scale(anim);
                float cx = (float)tmp.frame_rect.x + (float)tmp.frame_rect.w * 0.5f;
                float cy = (float)tmp.frame_rect.y + (float)tmp.frame_rect.h * 0.5f;
                tmp.frame_rect.x = (int32_t)(cx - (float)tmp.frame_rect.w * scale * 0.5f);
                tmp.frame_rect.y = (int32_t)(cy - (float)tmp.frame_rect.h * scale * 0.5f);
                tmp.frame_rect.w = (int32_t)((float)tmp.frame_rect.w * scale);
                tmp.frame_rect.h = (int32_t)((float)tmp.frame_rect.h * scale);
                tmp.content_rect = vgp_window_content_rect(&tmp.frame_rect, theme);
            } else if (anim->type == VGP_ANIM_WINDOW_MAXIMIZE ||
                       anim->type == VGP_ANIM_WINDOW_RESTORE) {
                /* Smooth geometry interpolation for maximize/restore */
                float ax, ay, aw, ah;
                vgp_anim_rect(anim, &ax, &ay, &aw, &ah);
                tmp.frame_rect.x = (int32_t)ax - out_x;
                tmp.frame_rect.y = (int32_t)ay;
                tmp.frame_rect.w = (int32_t)aw;
                tmp.frame_rect.h = (int32_t)ah;
                tmp.content_rect = vgp_window_content_rect(&tmp.frame_rect, theme);
            }
        }

        /* Skip if fully transparent */
        if (win_opacity < 0.01f) continue;

        /* Soft shadow under glass pane */
        if (win->decorated) {
            float cr = theme->corner_radius > 0 ? theme->corner_radius : 10.0f;
            float sh_offset = 4.0f;
            float sh_spread = 16.0f;
            float sh_alpha = 0.2f * win_opacity;
            /* Outer diffuse shadow */
            b->ops->draw_rounded_rect(b, ctx,
                (float)tmp.frame_rect.x - sh_spread + sh_offset,
                (float)tmp.frame_rect.y - sh_spread + sh_offset,
                (float)tmp.frame_rect.w + sh_spread * 2,
                (float)tmp.frame_rect.h + sh_spread * 2,
                cr + sh_spread * 0.5f,
                0, 0, 0, sh_alpha * 0.5f);
            /* Inner sharper shadow */
            b->ops->draw_rounded_rect(b, ctx,
                (float)tmp.frame_rect.x - 4 + sh_offset,
                (float)tmp.frame_rect.y - 4 + sh_offset,
                (float)tmp.frame_rect.w + 8,
                (float)tmp.frame_rect.h + 8,
                cr + 2,
                0, 0, 0, sh_alpha);
        }

        /* Render with opacity */
        b->ops->push_state(b, ctx);
        render_decoration(b, ctx, &tmp, theme, win == comp->focused);
        render_window_content(b, ctx, win, out_x);

        /* Accessibility: bright focus indicator ring (outline only) */
        if (renderer->focus_indicator && win == comp->focused && win->decorated) {
            float fi_w = 3.0f;
            float fx = (float)tmp.frame_rect.x - fi_w;
            float fy = (float)tmp.frame_rect.y - fi_w;
            float fw = (float)tmp.frame_rect.w + fi_w * 2;
            float fh = (float)tmp.frame_rect.h + fi_w * 2;
            /* Draw 4 border lines instead of a filled rect */
            b->ops->draw_rect(b, ctx, fx, fy, fw, fi_w, 1.0f, 0.8f, 0.0f, 0.9f);           /* top */
            b->ops->draw_rect(b, ctx, fx, fy + fh - fi_w, fw, fi_w, 1.0f, 0.8f, 0.0f, 0.9f); /* bottom */
            b->ops->draw_rect(b, ctx, fx, fy, fi_w, fh, 1.0f, 0.8f, 0.0f, 0.9f);             /* left */
            b->ops->draw_rect(b, ctx, fx + fw - fi_w, fy, fi_w, fh, 1.0f, 0.8f, 0.0f, 0.9f); /* right */
        }

        b->ops->pop_state(b, ctx);
    }

    if (need_translate)
        b->ops->pop_state(b, ctx);

    /* Layer 2: Panel */
    vgp_panel_render(b, ctx, theme, panel_cfg,
                      output->width, output->height, workspace, comp);

    /* Layer 3: Cursor (only on the output where the cursor is) */
    {
        float cx = comp->cursor.x;
        float cy = comp->cursor.y;
        float ox = (float)out_x;
        float ow = (float)output->width;
        float oh = (float)output->height;
        if (cx >= ox && cx < ox + ow && cy >= 0 && cy < oh) {
            /* Translate cursor to output-local coordinates */
            vgp_cursor_t local_cursor = comp->cursor;
            local_cursor.x -= ox;
            render_cursor(b, ctx, &local_cursor);
        }
    }

    /* Layer 4: Notifications (top-right, on active output only) */
    if (notify && notify->count > 0 && output_idx == comp->active_output)
        vgp_notify_render(notify, b, ctx,
                           (float)output->width, (float)output->height,
                           theme->statusbar_font_size);

    /* Layer 5: Context menu (on active output) */
    if (menu && menu->visible && output_idx == comp->active_output)
        vgp_menu_render(menu, b, ctx, theme->statusbar_font_size);

    /* Layer 5b: Calendar popup */
    if (cal && cal->visible && output_idx == comp->active_output)
        vgp_calendar_render(cal, b, ctx, theme->statusbar_font_size);

    /* Layer 6: Lock screen (covers everything when locked) */
    if (lock && vgp_lockscreen_is_locked(lock)) {
        static float lock_time = 0;
        lock_time += 0.016f;
        vgp_lockscreen_render(lock, b, ctx,
                               (float)output->width, (float)output->height,
                               lock_time);
    }

    /* End frame */
    b->ops->end_frame(b, output_idx, output);

    /* Present to screen */
    uint32_t fb_id = b->ops->get_fb_id(b, output_idx, output);
    if (!fb_id) return;

    if (b->type == VGP_BACKEND_GPU && !renderer->gpu_crtc_owned[output_idx]) {
        /* First GPU frame: blocking modeset to take CRTC ownership.
         * No dumb buffers were created (skip_dumb_buffers=true), so
         * the CRTC has no prior modeset. This is the initial setup. */
        int ret = drmModeSetCrtc(drm->drm_fd, output->crtc_id, fb_id,
                                  0, 0, &output->connector_id, 1, &output->mode);
        if (ret == 0) {
            renderer->gpu_crtc_owned[output_idx] = true;
            VGP_LOG_INFO(TAG, "GPU took CRTC for output %d", output_idx);
        }
    } else {
        /* Normal: async page flip (vblank-synced, non-blocking) */
        int ret = drmModePageFlip(drm->drm_fd, output->crtc_id, fb_id,
                                   DRM_MODE_PAGE_FLIP_EVENT, output);
        if (ret == 0) {
            output->page_flip_pending = true;
        } else if (errno == EBUSY) {
            /* Flip already pending or CRTC busy -- use SetCrtc as fallback */
            drmModeSetCrtc(drm->drm_fd, output->crtc_id, fb_id,
                            0, 0, &output->connector_id, 1, &output->mode);
        }
    }
}

static void frame_timer_callback(void *data)
{
    struct vgp_server *server = data;
    extern void vgp_server_render_frame(struct vgp_server *server);
    vgp_server_render_frame(server);
}
