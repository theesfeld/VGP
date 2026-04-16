#include "renderer.h"
#include "render_backend.h"
#include "window.h"
#include "notify.h"
#include "animation.h"
#include "lockscreen.h"
#include "menu.h"
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
    float bw = theme->border_width;
    float cr = theme->corner_radius;

    /* Border */
    const vgp_color_t *bc = focused ? &theme->border_active : &theme->border_inactive;
    b->ops->draw_rounded_rect(b, ctx, x, y, w, h, cr,
                               bc->r, bc->g, bc->b, bc->a);

    /* Title bar fill (slightly inset) */
    const vgp_color_t *tb = focused ? &theme->titlebar_active : &theme->titlebar_inactive;
    b->ops->draw_rounded_rect(b, ctx, x + bw, y + bw,
                               w - bw * 2, th - bw, cr > bw ? cr - bw : 1,
                               tb->r, tb->g, tb->b, tb->a);

    /* Content area background */
    const vgp_color_t *cb = &theme->content_bg;
    b->ops->draw_rect(b, ctx, x + bw, y + th,
                       w - bw * 2, h - th - bw,
                       cb->r, cb->g, cb->b, cb->a);

    /* Title text */
    if (win->title[0]) {
        const vgp_color_t *tc = focused ? &theme->title_text_active
                                        : &theme->title_text_inactive;
        float text_x = x + bw + 10.0f;
        float text_y = y + th / 2.0f + theme->title_font_size / 3.0f;
        b->ops->draw_text(b, ctx, win->title, -1, text_x, text_y,
                           theme->title_font_size,
                           tc->r, tc->g, tc->b, tc->a);
    }

    /* Buttons */
    float btn_r = theme->button_radius;
    float btn_spacing = theme->button_spacing;
    float btn_margin = theme->button_margin_right;
    float btn_cy = y + th / 2.0f;

    /* Close */
    float close_cx = x + w - btn_margin - btn_r;
    b->ops->draw_circle(b, ctx, close_cx, btn_cy, btn_r,
                         theme->close_btn.r, theme->close_btn.g,
                         theme->close_btn.b, theme->close_btn.a);

    /* Maximize */
    float max_cx = close_cx - btn_r * 2 - btn_spacing;
    b->ops->draw_circle(b, ctx, max_cx, btn_cy, btn_r,
                         theme->maximize_btn.r, theme->maximize_btn.g,
                         theme->maximize_btn.b, theme->maximize_btn.a);

    /* Minimize */
    float min_cx = max_cx - btn_r * 2 - btn_spacing;
    b->ops->draw_circle(b, ctx, min_cx, btn_cy, btn_r,
                         theme->minimize_btn.r, theme->minimize_btn.g,
                         theme->minimize_btn.b, theme->minimize_btn.a);
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

static void render_window_content(vgp_render_backend_t *b, void *ctx,
                                   vgp_window_t *win, int32_t offset_x)
{
    vgp_rect_t content = win->content_rect;
    content.x -= offset_x;

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

static void render_statusbar(vgp_render_backend_t *b, void *ctx,
                              const vgp_theme_t *theme,
                              uint32_t width, uint32_t height,
                              int workspace,
                              vgp_compositor_t *comp)
{
    float bar_h = theme->statusbar_height;
    float bar_y = (float)height - bar_h;
    float fs = theme->statusbar_font_size;
    float text_y = bar_y + bar_h / 2.0f + fs / 3.0f;
    float pad = 6.0f;

    const vgp_color_t *bg = &theme->statusbar_bg;
    const vgp_color_t *ac = &theme->border_active;
    const vgp_color_t *tc = &theme->statusbar_text;

    /* Panel background */
    b->ops->draw_rect(b, ctx, 0, bar_y, (float)width, bar_h,
                       bg->r, bg->g, bg->b, bg->a);

    /* Top border line */
    b->ops->draw_line(b, ctx, 0, bar_y, (float)width, bar_y, 1.0f,
                       ac->r, ac->g, ac->b, 0.5f);

    float x_cursor = pad;

    /* === LEFT: Workspace indicators === */
    int ws_count = 9;
    for (int ws = 0; ws < ws_count; ws++) {
        /* Check if any window is on this workspace */
        bool has_windows = false;
        for (int i = 0; i < comp->window_count; i++) {
            if (comp->z_order[i]->workspace == ws && comp->z_order[i]->visible) {
                has_windows = true;
                break;
            }
        }

        bool is_active = (ws == workspace);
        float btn_w = 22.0f;
        float btn_h = bar_h - 8.0f;
        float btn_y = bar_y + 4.0f;

        if (is_active) {
            b->ops->draw_rounded_rect(b, ctx, x_cursor, btn_y, btn_w, btn_h, 3.0f,
                                       ac->r, ac->g, ac->b, 0.8f);
            char num[4];
            snprintf(num, sizeof(num), "%d", ws + 1);
            b->ops->draw_text(b, ctx, num, -1, x_cursor + 7, text_y, fs,
                               0.0f, 0.0f, 0.0f, 1.0f);
        } else if (has_windows) {
            b->ops->draw_rounded_rect(b, ctx, x_cursor, btn_y, btn_w, btn_h, 3.0f,
                                       tc->r * 0.3f, tc->g * 0.3f, tc->b * 0.3f, 0.5f);
            char num[4];
            snprintf(num, sizeof(num), "%d", ws + 1);
            b->ops->draw_text(b, ctx, num, -1, x_cursor + 7, text_y, fs,
                               tc->r, tc->g, tc->b, 0.8f);
        } else {
            char num[4];
            snprintf(num, sizeof(num), "%d", ws + 1);
            b->ops->draw_text(b, ctx, num, -1, x_cursor + 7, text_y, fs,
                               tc->r * 0.4f, tc->g * 0.4f, tc->b * 0.4f, 0.4f);
        }
        x_cursor += btn_w + 2.0f;
    }

    x_cursor += pad;

    /* Separator */
    b->ops->draw_line(b, ctx, x_cursor, bar_y + 6, x_cursor, bar_y + bar_h - 6, 1.0f,
                       tc->r, tc->g, tc->b, 0.2f);
    x_cursor += pad * 2;

    /* === CENTER: Taskbar (window list for current workspace) === */
    float taskbar_start = x_cursor;
    float taskbar_end = (float)width - 120.0f; /* leave room for clock */
    float taskbar_w = taskbar_end - taskbar_start;

    /* Count windows on this workspace */
    int win_count = 0;
    for (int i = 0; i < comp->window_count; i++) {
        vgp_window_t *w = comp->z_order[i];
        if (w->visible && w->workspace == workspace && w->decorated)
            win_count++;
    }

    if (win_count > 0 && taskbar_w > 0) {
        float entry_w = taskbar_w / (float)win_count;
        if (entry_w > 250.0f) entry_w = 250.0f;
        float ex = taskbar_start;

        for (int i = 0; i < comp->window_count; i++) {
            vgp_window_t *w = comp->z_order[i];
            if (!w->visible || w->workspace != workspace || !w->decorated)
                continue;

            bool is_focused = (w == comp->focused);
            float ew = entry_w - 4.0f;
            float eh = bar_h - 8.0f;
            float ey = bar_y + 4.0f;

            /* Hover detection */
            int32_t out_offset = 0;
            for (int oi = 0; oi < comp->output_count; oi++) {
                if (comp->outputs[oi].workspace == workspace) {
                    out_offset = comp->outputs[oi].x;
                    break;
                }
            }
            float local_mx = comp->cursor.x - (float)out_offset;
            float local_my = comp->cursor.y;
            bool is_hover = (local_my >= ey && local_my < ey + eh &&
                              local_mx >= ex + 2 && local_mx < ex + 2 + ew);

            /* Entry background */
            if (is_focused) {
                b->ops->draw_rounded_rect(b, ctx, ex + 2, ey, ew, eh, 4.0f,
                                           ac->r * 0.3f, ac->g * 0.3f, ac->b * 0.3f, 0.6f);
                /* Active indicator bar at bottom */
                b->ops->draw_rounded_rect(b, ctx, ex + 6, ey + eh - 3, ew - 8, 2.0f, 1.0f,
                                           ac->r, ac->g, ac->b, 1.0f);
            } else if (is_hover) {
                b->ops->draw_rounded_rect(b, ctx, ex + 2, ey, ew, eh, 4.0f,
                                           ac->r * 0.15f, ac->g * 0.15f, ac->b * 0.15f, 0.4f);
            } else {
                b->ops->draw_rounded_rect(b, ctx, ex + 2, ey, ew, eh, 4.0f,
                                           tc->r * 0.1f, tc->g * 0.1f, tc->b * 0.1f, 0.3f);
            }

            /* Window title (truncated) */
            float max_text_w = ew - 12.0f;
            int max_chars = (int)(max_text_w / (fs * 0.55f));
            if (max_chars > 0) {
                char truncated[64];
                int title_len = (int)strlen(w->title);
                if (title_len > max_chars && max_chars > 3) {
                    snprintf(truncated, sizeof(truncated), "%.*s...",
                             max_chars - 3, w->title);
                } else {
                    snprintf(truncated, sizeof(truncated), "%.*s",
                             max_chars, w->title);
                }
                float text_alpha = is_focused ? 1.0f : 0.6f;
                b->ops->draw_text(b, ctx, truncated, -1,
                                   ex + 8, text_y, fs - 1,
                                   tc->r, tc->g, tc->b, text_alpha);
            }

            ex += entry_w;
        }
    }

    /* Separator before clock */
    float sep_x = (float)width - 115.0f;
    b->ops->draw_line(b, ctx, sep_x, bar_y + 6, sep_x, bar_y + bar_h - 6, 1.0f,
                       tc->r, tc->g, tc->b, 0.2f);

    /* === RIGHT: Clock + date === */
    {
        time_t now = time(NULL);
        struct tm *tm_now = localtime(&now);
        char clock_buf[32];
        snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d",
                 tm_now->tm_hour, tm_now->tm_min);
        b->ops->draw_text(b, ctx, clock_buf, -1,
                           (float)width - 100.0f, text_y, fs,
                           tc->r, tc->g, tc->b, tc->a);

        char date_buf[32];
        snprintf(date_buf, sizeof(date_buf), "%02d/%02d",
                 tm_now->tm_mon + 1, tm_now->tm_mday);
        b->ops->draw_text(b, ctx, date_buf, -1,
                           (float)width - 50.0f, text_y, fs - 2,
                           tc->r * 0.7f, tc->g * 0.7f, tc->b * 0.7f, 0.7f);
    }
}

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
        }
    }
#endif

    VGP_LOG_INFO(TAG, "renderer initialized (bg_shader=%d, panel_shader=%d)",
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
                                 struct vgp_menu *menu)
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

    /* For GPU backend, we need to translate window coordinates
     * relative to this output's position in the global layout */
    bool need_translate = (out_x != 0);
    if (need_translate) {
        b->ops->push_state(b, ctx);
        /* Shift everything left by the output's x offset so windows
         * on this output appear at the correct position */
    }

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

        /* Translate window coordinates to output-local space */
        vgp_window_t tmp = *win;
        tmp.frame_rect.x -= out_x;
        tmp.content_rect.x -= out_x;

        /* Check for active animation on this window */
        float win_opacity = (win == comp->focused) ?
            theme->window_opacity : theme->inactive_opacity;
        if (win_opacity <= 0.0f) win_opacity = 1.0f; /* sanity */

        vgp_animation_t *anim = anims ? vgp_anim_find(anims, win->id) : NULL;
        if (anim) {
            win_opacity *= vgp_anim_opacity(anim);
            /* Animation can modify geometry for open/close scale */
            if (anim->type == VGP_ANIM_WINDOW_OPEN || anim->type == VGP_ANIM_WINDOW_CLOSE) {
                float scale = vgp_anim_scale(anim);
                float cx = (float)tmp.frame_rect.x + (float)tmp.frame_rect.w * 0.5f;
                float cy = (float)tmp.frame_rect.y + (float)tmp.frame_rect.h * 0.5f;
                tmp.frame_rect.x = (int32_t)(cx - (float)tmp.frame_rect.w * scale * 0.5f);
                tmp.frame_rect.y = (int32_t)(cy - (float)tmp.frame_rect.h * scale * 0.5f);
                tmp.frame_rect.w = (int32_t)((float)tmp.frame_rect.w * scale);
                tmp.frame_rect.h = (int32_t)((float)tmp.frame_rect.h * scale);
                tmp.content_rect = vgp_window_content_rect(&tmp.frame_rect, theme);
            }
        }

        /* Skip if fully transparent */
        if (win_opacity < 0.01f) continue;

        /* Drop shadow (behind window, slightly offset) */
        if (win->decorated) {
            float sh_offset = 6.0f;
            float sh_spread = 12.0f;
            float sh_alpha = 0.3f * win_opacity;
            b->ops->draw_rounded_rect(b, ctx,
                (float)tmp.frame_rect.x - sh_spread + sh_offset,
                (float)tmp.frame_rect.y - sh_spread + sh_offset,
                (float)tmp.frame_rect.w + sh_spread * 2,
                (float)tmp.frame_rect.h + sh_spread * 2,
                theme->corner_radius + sh_spread * 0.5f,
                0, 0, 0, sh_alpha);
        }

        /* Render with opacity (TODO: proper alpha group when NanoVG supports it) */
        b->ops->push_state(b, ctx);
        render_decoration(b, ctx, &tmp, theme, win == comp->focused);
        render_window_content(b, ctx, win, out_x);
        b->ops->pop_state(b, ctx);
    }

    if (need_translate)
        b->ops->pop_state(b, ctx);

    /* Layer 2: Panel with workspace indicators + taskbar + clock */
    render_statusbar(b, ctx, theme, output->width, output->height,
                      workspace, comp);

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
