/* SPDX-License-Identifier: MIT */
#ifndef VGP_DESKTOP_H
#define VGP_DESKTOP_H

#include "theme.h"
#include <plutovg.h>

void vgp_desktop_render_background(plutovg_canvas_t *canvas,
                                    const vgp_theme_t *theme,
                                    uint32_t width, uint32_t height);

void vgp_desktop_render_statusbar(plutovg_canvas_t *canvas,
                                   plutovg_font_face_t *font_face,
                                   const vgp_theme_t *theme,
                                   uint32_t width, uint32_t height,
                                   const char *focused_title);

#endif /* VGP_DESKTOP_H */