#include "decoration.h"

void vgp_decoration_render(const vgp_window_t *win,
                            plutovg_canvas_t *canvas,
                            plutovg_font_face_t *font_face,
                            const vgp_theme_t *theme,
                            bool focused)
{
    if (!win->decorated)
        return;

    const vgp_rect_t *f = &win->frame_rect;
    float x = (float)f->x;
    float y = (float)f->y;
    float w = (float)f->w;
    float h = (float)f->h;
    float th = theme->titlebar_height;
    float bw = theme->border_width;
    float cr = theme->corner_radius;

    plutovg_canvas_save(canvas);

    /* === Border / frame outline === */
    const vgp_color_t *border_color = focused ?
        &theme->border_active : &theme->border_inactive;
    plutovg_canvas_set_rgba(canvas, border_color->r, border_color->g,
                             border_color->b, border_color->a);
    plutovg_canvas_round_rect(canvas, x, y, w, h, cr, cr);
    plutovg_canvas_set_line_width(canvas, bw);
    plutovg_canvas_stroke(canvas);

    /* === Title bar background === */
    const vgp_color_t *tb_color = focused ?
        &theme->titlebar_active : &theme->titlebar_inactive;
    plutovg_canvas_set_rgba(canvas, tb_color->r, tb_color->g,
                             tb_color->b, tb_color->a);

    /* Draw titlebar as top portion with rounded top corners */
    plutovg_canvas_move_to(canvas, x + cr, y);
    plutovg_canvas_line_to(canvas, x + w - cr, y);
    plutovg_canvas_arc(canvas, x + w - cr, y + cr, cr, -3.14159265f / 2.0f, 0, false);
    plutovg_canvas_line_to(canvas, x + w, y + th);
    plutovg_canvas_line_to(canvas, x, y + th);
    plutovg_canvas_line_to(canvas, x, y + cr);
    plutovg_canvas_arc(canvas, x + cr, y + cr, cr, 3.14159265f, -3.14159265f / 2.0f, false);
    plutovg_canvas_close_path(canvas);
    plutovg_canvas_fill(canvas);

    /* === Content area background === */
    const vgp_color_t *content_color = &theme->content_bg;
    plutovg_canvas_set_rgba(canvas, content_color->r, content_color->g,
                             content_color->b, content_color->a);

    /* Bottom portion with rounded bottom corners */
    plutovg_canvas_move_to(canvas, x, y + th);
    plutovg_canvas_line_to(canvas, x + w, y + th);
    plutovg_canvas_line_to(canvas, x + w, y + h - cr);
    plutovg_canvas_arc(canvas, x + w - cr, y + h - cr, cr, 0, 3.14159265f / 2.0f, false);
    plutovg_canvas_line_to(canvas, x + cr, y + h);
    plutovg_canvas_arc(canvas, x + cr, y + h - cr, cr, 3.14159265f / 2.0f, 3.14159265f, false);
    plutovg_canvas_close_path(canvas);
    plutovg_canvas_fill(canvas);

    /* === Title text === */
    if (win->title[0] != '\0' && font_face) {
        const vgp_color_t *text_color = focused ?
            &theme->title_text_active : &theme->title_text_inactive;
        plutovg_canvas_set_rgba(canvas, text_color->r, text_color->g,
                                 text_color->b, text_color->a);
        plutovg_canvas_set_font(canvas, font_face, theme->title_font_size);

        float text_x = x + bw + 10.0f;
        float text_y = y + th / 2.0f + theme->title_font_size / 3.0f;
        plutovg_canvas_fill_text(canvas, win->title, -1,
                                  PLUTOVG_TEXT_ENCODING_UTF8,
                                  text_x, text_y);
    }

    /* === Buttons === */
    float btn_r = theme->button_radius;
    float btn_spacing = theme->button_spacing;
    float btn_margin = theme->button_margin_right;
    float btn_cy = y + th / 2.0f;

    /* Close button (rightmost) */
    float close_cx = x + w - btn_margin - btn_r;
    plutovg_canvas_set_rgba(canvas, theme->close_btn.r, theme->close_btn.g,
                             theme->close_btn.b, theme->close_btn.a);
    plutovg_canvas_circle(canvas, close_cx, btn_cy, btn_r);
    plutovg_canvas_fill(canvas);

    /* X icon inside close button */
    float icon_size = btn_r * 0.45f;
    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_set_line_width(canvas, 1.5f);
    plutovg_canvas_move_to(canvas, close_cx - icon_size, btn_cy - icon_size);
    plutovg_canvas_line_to(canvas, close_cx + icon_size, btn_cy + icon_size);
    plutovg_canvas_move_to(canvas, close_cx + icon_size, btn_cy - icon_size);
    plutovg_canvas_line_to(canvas, close_cx - icon_size, btn_cy + icon_size);
    plutovg_canvas_stroke(canvas);

    /* Maximize button */
    float max_cx = close_cx - btn_r * 2 - btn_spacing;
    plutovg_canvas_set_rgba(canvas, theme->maximize_btn.r, theme->maximize_btn.g,
                             theme->maximize_btn.b, theme->maximize_btn.a);
    plutovg_canvas_circle(canvas, max_cx, btn_cy, btn_r);
    plutovg_canvas_fill(canvas);

    /* Square icon inside maximize button */
    float sq = btn_r * 0.4f;
    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_set_line_width(canvas, 1.2f);
    plutovg_canvas_rect(canvas, max_cx - sq, btn_cy - sq, sq * 2, sq * 2);
    plutovg_canvas_stroke(canvas);

    /* Minimize button */
    float min_cx = max_cx - btn_r * 2 - btn_spacing;
    plutovg_canvas_set_rgba(canvas, theme->minimize_btn.r, theme->minimize_btn.g,
                             theme->minimize_btn.b, theme->minimize_btn.a);
    plutovg_canvas_circle(canvas, min_cx, btn_cy, btn_r);
    plutovg_canvas_fill(canvas);

    /* Line icon inside minimize button */
    plutovg_canvas_set_rgb(canvas, 0, 0, 0);
    plutovg_canvas_set_line_width(canvas, 1.5f);
    plutovg_canvas_move_to(canvas, min_cx - icon_size, btn_cy);
    plutovg_canvas_line_to(canvas, min_cx + icon_size, btn_cy);
    plutovg_canvas_stroke(canvas);

    plutovg_canvas_restore(canvas);
}

void vgp_window_render_content(const vgp_window_t *win,
                                plutovg_canvas_t *canvas)
{
    if (!win->client_surface)
        return;

    const vgp_rect_t *c = &win->content_rect;

    plutovg_canvas_save(canvas);

    /* Clip to content area */
    plutovg_canvas_rect(canvas, (float)c->x, (float)c->y,
                         (float)c->w, (float)c->h);
    plutovg_canvas_clip(canvas);

    /* Set up texture from client surface */
    plutovg_matrix_t mat;
    plutovg_matrix_init_translate(&mat, -(float)c->x, -(float)c->y);
    plutovg_canvas_set_texture(canvas, win->client_surface,
                                PLUTOVG_TEXTURE_TYPE_PLAIN,
                                1.0f, &mat);

    plutovg_canvas_rect(canvas, (float)c->x, (float)c->y,
                         (float)c->w, (float)c->h);
    plutovg_canvas_fill(canvas);

    plutovg_canvas_restore(canvas);
}
