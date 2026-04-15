#ifndef VGP_THEME_H
#define VGP_THEME_H

#include "vgp/types.h"

typedef struct vgp_theme {
    /* Title bar */
    float titlebar_height;
    float border_width;
    float corner_radius;

    /* Buttons */
    float button_radius;
    float button_spacing;
    float button_margin_right;

    /* Colors */
    vgp_color_t titlebar_active;
    vgp_color_t titlebar_inactive;
    vgp_color_t border_active;
    vgp_color_t border_inactive;
    vgp_color_t title_text_active;
    vgp_color_t title_text_inactive;
    vgp_color_t close_btn;
    vgp_color_t maximize_btn;
    vgp_color_t minimize_btn;
    vgp_color_t close_btn_hover;
    vgp_color_t background;
    vgp_color_t statusbar_bg;
    vgp_color_t statusbar_text;
    vgp_color_t content_bg;

    /* Font sizes */
    float title_font_size;
    float statusbar_font_size;
    float statusbar_height;

    /* Background: 0=solid color, 1=shader, 2=wallpaper, 3=none (black) */
    int   background_mode;
    char  background_shader[256];
    char  background_wallpaper[256];

    /* Window transparency (0.0 = fully transparent, 1.0 = opaque) */
    float window_opacity;       /* default 0.9 = 90% */
    float inactive_opacity;     /* default 0.85 for unfocused windows */
} vgp_theme_t;

/* Load defaults (always succeeds) */
void vgp_theme_load_defaults(vgp_theme_t *theme);

/* Load from file (falls back to defaults on failure) */
int  vgp_theme_load(vgp_theme_t *theme, const char *path);

#endif /* VGP_THEME_H */
