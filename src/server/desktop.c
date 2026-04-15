#include "desktop.h"

#include <stdio.h>
#include <time.h>

void vgp_desktop_render_background(plutovg_canvas_t *canvas,
                                    const vgp_theme_t *theme,
                                    uint32_t width, uint32_t height)
{
    /* Simple solid background */
    plutovg_canvas_set_rgba(canvas, theme->background.r, theme->background.g,
                             theme->background.b, theme->background.a);
    plutovg_canvas_rect(canvas, 0, 0, (float)width, (float)height);
    plutovg_canvas_fill(canvas);
}

void vgp_desktop_render_statusbar(plutovg_canvas_t *canvas,
                                   plutovg_font_face_t *font_face,
                                   const vgp_theme_t *theme,
                                   uint32_t width, uint32_t height,
                                   const char *focused_title)
{
    float bar_h = theme->statusbar_height;
    float bar_y = (float)height - bar_h;

    plutovg_canvas_save(canvas);

    /* Background */
    plutovg_canvas_set_rgba(canvas, theme->statusbar_bg.r,
                             theme->statusbar_bg.g, theme->statusbar_bg.b,
                             theme->statusbar_bg.a);
    plutovg_canvas_rect(canvas, 0, bar_y, (float)width, bar_h);
    plutovg_canvas_fill(canvas);

    /* Top border line */
    plutovg_canvas_set_rgba(canvas, theme->border_active.r,
                             theme->border_active.g, theme->border_active.b,
                             0.5f);
    plutovg_canvas_move_to(canvas, 0, bar_y);
    plutovg_canvas_line_to(canvas, (float)width, bar_y);
    plutovg_canvas_set_line_width(canvas, 1.0f);
    plutovg_canvas_stroke(canvas);

    if (!font_face)
        goto done;

    plutovg_canvas_set_font(canvas, font_face, theme->statusbar_font_size);
    float text_y = bar_y + bar_h / 2.0f + theme->statusbar_font_size / 3.0f;

    /* Left: VGP label */
    plutovg_canvas_set_rgba(canvas, theme->border_active.r,
                             theme->border_active.g, theme->border_active.b,
                             theme->border_active.a);
    plutovg_canvas_fill_text(canvas, " VGP ", -1,
                              PLUTOVG_TEXT_ENCODING_UTF8, 8.0f, text_y);

    /* Center: focused window title */
    if (focused_title && focused_title[0]) {
        plutovg_canvas_set_rgba(canvas, theme->statusbar_text.r,
                                 theme->statusbar_text.g, theme->statusbar_text.b,
                                 theme->statusbar_text.a);

        /* Approximate centering */
        float approx_title_w = (float)strlen(focused_title) * theme->statusbar_font_size * 0.55f;
        float title_x = ((float)width - approx_title_w) / 2.0f;
        if (title_x < 80.0f) title_x = 80.0f;
        plutovg_canvas_fill_text(canvas, focused_title, -1,
                                  PLUTOVG_TEXT_ENCODING_UTF8, title_x, text_y);
    }

    /* Right: clock */
    {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char clock_buf[16];
        snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d", tm->tm_hour, tm->tm_min);

        plutovg_canvas_set_rgba(canvas, theme->statusbar_text.r,
                                 theme->statusbar_text.g, theme->statusbar_text.b,
                                 theme->statusbar_text.a);
        float clock_x = (float)width - 60.0f;
        plutovg_canvas_fill_text(canvas, clock_buf, -1,
                                  PLUTOVG_TEXT_ENCODING_UTF8, clock_x, text_y);
    }

done:
    plutovg_canvas_restore(canvas);
}
