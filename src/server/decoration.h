/* SPDX-License-Identifier: MIT */
#ifndef VGP_DECORATION_H
#define VGP_DECORATION_H

#include "window.h"
#include "theme.h"
#include <plutovg.h>

/* Render window decorations (title bar, borders, buttons) */
void vgp_decoration_render(const vgp_window_t *win,
                            plutovg_canvas_t *canvas,
                            plutovg_font_face_t *font_face,
                            const vgp_theme_t *theme,
                            bool focused);

/* Render client surface content into the window content area */
void vgp_window_render_content(const vgp_window_t *win,
                                plutovg_canvas_t *canvas);

#endif /* VGP_DECORATION_H */