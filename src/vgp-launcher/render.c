#include "launcher.h"
#include <stdio.h>
#include <string.h>

void launcher_render(launcher_t *l)
{
    float w = LAUNCHER_WIDTH, h = LAUNCHER_HEIGHT;
    float pad = l->padding;
    float item_h = l->item_height;
    float input_h = 36.0f;

    /* Clear */
    plutovg_color_t bg;
    plutovg_color_init_rgba(&bg, 0.08f, 0.08f, 0.12f, 0.97f);
    plutovg_surface_clear(l->surface, &bg);

    /* Border */
    plutovg_canvas_set_rgba(l->canvas, 0.32f, 0.53f, 0.88f, 0.8f);
    plutovg_canvas_round_rect(l->canvas, 1, 1, w - 2, h - 2, 12, 12);
    plutovg_canvas_set_line_width(l->canvas, 2.0f);
    plutovg_canvas_stroke(l->canvas);

    /* Title */
    if (l->font_face) {
        plutovg_canvas_set_font(l->canvas, l->font_face, l->font_size + 2);
        plutovg_canvas_set_rgba(l->canvas, 0.32f, 0.53f, 0.88f, 1.0f);
        plutovg_canvas_fill_text(l->canvas, "VGP Launcher", -1,
                                  PLUTOVG_TEXT_ENCODING_UTF8,
                                  pad + 8, pad + 16);
    }

    /* Input box */
    float box_y = pad + 28;
    plutovg_canvas_set_rgba(l->canvas, 0.14f, 0.14f, 0.2f, 1.0f);
    plutovg_canvas_round_rect(l->canvas, pad, box_y,
                               w - pad * 2, input_h, 6, 6);
    plutovg_canvas_fill(l->canvas);

    /* Input box border */
    plutovg_canvas_set_rgba(l->canvas, 0.32f, 0.53f, 0.88f, 0.5f);
    plutovg_canvas_round_rect(l->canvas, pad, box_y,
                               w - pad * 2, input_h, 6, 6);
    plutovg_canvas_set_line_width(l->canvas, 1.0f);
    plutovg_canvas_stroke(l->canvas);

    /* Input text */
    if (l->font_face) {
        float text_y = box_y + input_h / 2.0f + l->font_size / 3.0f;
        plutovg_canvas_set_font(l->canvas, l->font_face, l->font_size);

        if (l->input_len > 0) {
            plutovg_canvas_set_rgba(l->canvas, 0.92f, 0.92f, 0.92f, 1.0f);
            plutovg_canvas_fill_text(l->canvas, l->input_buf, l->input_len,
                                      PLUTOVG_TEXT_ENCODING_UTF8,
                                      pad + 10, text_y);
        } else {
            /* Placeholder */
            plutovg_canvas_set_rgba(l->canvas, 0.45f, 0.45f, 0.5f, 1.0f);
            plutovg_canvas_fill_text(l->canvas, "Type to search...", -1,
                                      PLUTOVG_TEXT_ENCODING_UTF8,
                                      pad + 10, text_y);
        }

        /* Text cursor */
        float cursor_x = pad + 10;
        if (l->input_len > 0) {
            cursor_x += plutovg_font_face_text_extents(l->font_face, l->font_size,
                                                        l->input_buf, l->input_len,
                                                        PLUTOVG_TEXT_ENCODING_UTF8,
                                                        NULL);
        }
        plutovg_canvas_set_rgba(l->canvas, 0.92f, 0.92f, 0.92f, 0.8f);
        plutovg_canvas_rect(l->canvas, cursor_x, box_y + 6, 2, input_h - 12);
        plutovg_canvas_fill(l->canvas);
    }

    /* Results list */
    float list_y = box_y + input_h + pad;

    /* Result count */
    if (l->font_face) {
        char count_str[32];
        snprintf(count_str, sizeof(count_str), "%d results", l->filtered_count);
        plutovg_canvas_set_font(l->canvas, l->font_face, l->font_size - 2);
        plutovg_canvas_set_rgba(l->canvas, 0.45f, 0.45f, 0.5f, 1.0f);
        plutovg_canvas_fill_text(l->canvas, count_str, -1,
                                  PLUTOVG_TEXT_ENCODING_UTF8,
                                  w - pad - 80, list_y - 4);
    }

    for (int i = 0; i < LAUNCHER_VISIBLE_ITEMS &&
         (i + l->scroll_offset) < l->filtered_count; i++) {
        int idx = i + l->scroll_offset;
        launcher_app_t *app = &l->app_list.apps[l->filtered[idx].app_index];

        float iy = list_y + (float)i * item_h;

        /* Selection highlight */
        if (idx == l->selected_index) {
            plutovg_canvas_set_rgba(l->canvas, 0.32f, 0.53f, 0.88f, 0.25f);
            plutovg_canvas_round_rect(l->canvas, pad, iy,
                                       w - pad * 2, item_h, 4, 4);
            plutovg_canvas_fill(l->canvas);

            /* Selection left accent bar */
            plutovg_canvas_set_rgba(l->canvas, 0.32f, 0.53f, 0.88f, 0.9f);
            plutovg_canvas_round_rect(l->canvas, pad, iy + 4,
                                       3, item_h - 8, 1.5f, 1.5f);
            plutovg_canvas_fill(l->canvas);
        }

        /* App name */
        if (l->font_face) {
            float name_y = iy + item_h / 2.0f + l->font_size / 3.0f;

            if (idx == l->selected_index)
                plutovg_canvas_set_rgba(l->canvas, 0.95f, 0.95f, 0.95f, 1.0f);
            else
                plutovg_canvas_set_rgba(l->canvas, 0.78f, 0.78f, 0.78f, 1.0f);

            plutovg_canvas_set_font(l->canvas, l->font_face, l->font_size);
            plutovg_canvas_fill_text(l->canvas, app->name, -1,
                                      PLUTOVG_TEXT_ENCODING_UTF8,
                                      pad + 18, name_y);
        }
    }

    /* Commit surface to VGP */
    uint8_t *data = plutovg_surface_get_data(l->surface);
    int stride = plutovg_surface_get_stride(l->surface);
    fprintf(stderr, "  render: committing surface %dx%d stride=%d (%zu bytes)\n",
            LAUNCHER_WIDTH, LAUNCHER_HEIGHT, stride,
            (size_t)stride * LAUNCHER_HEIGHT);
    vgp_surface_attach(l->conn, l->window_id,
                        LAUNCHER_WIDTH, LAUNCHER_HEIGHT,
                        (uint32_t)stride, data);
    fprintf(stderr, "  render: surface_attach sent\n");
}
