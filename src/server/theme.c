#include "theme.h"
#include "vgp/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TAG "theme"

void vgp_theme_load_defaults(vgp_theme_t *theme)
{
    /* Geometry -- larger readable HUD style */
    theme->titlebar_height = 32.0f;
    theme->border_width = 1.0f;
    theme->corner_radius = 8.0f;
    theme->button_radius = 5.0f;
    theme->button_spacing = 12.0f;
    theme->button_margin_right = 16.0f;

    /* F-16 HUD colors: white/red/yellow, now over sky.
     * Glass picks up a faint blue tint; content stays dark for contrast. */
    theme->titlebar_active   = (vgp_color_t){0.60f, 0.75f, 0.95f, 0.08f}; /* cool glass, almost clear */
    theme->titlebar_inactive = (vgp_color_t){0.55f, 0.65f, 0.80f, 0.04f};
    theme->border_active     = vgp_color_hex(0xFFD700); /* yellow accent */
    theme->border_inactive   = vgp_color_hex(0x666666);
    theme->title_text_active   = vgp_color_hex(0xFFFFF2); /* near-white */
    theme->title_text_inactive = vgp_color_hex(0x888888);
    theme->close_btn         = vgp_color_hex(0xCCCCCC);
    theme->maximize_btn      = vgp_color_hex(0xCCCCCC);
    theme->minimize_btn      = vgp_color_hex(0xCCCCCC);
    theme->close_btn_hover   = vgp_color_hex(0xFF3333);
    theme->background        = vgp_color_hex(0x000000);
    theme->statusbar_bg      = (vgp_color_t){0.08f, 0.14f, 0.24f, 0.22f}; /* glass panel */
    theme->statusbar_text    = vgp_color_hex(0xFFFFF2);
    /* Content plate: low alpha so the sky shows clearly through.
     * Apps paint bright vector primitives on top -- HUD projection. */
    theme->content_bg        = (vgp_color_t){0.04f, 0.07f, 0.12f, 0.28f};

    /* Font sizes -- larger for readability */
    theme->title_font_size = 15.0f;
    theme->statusbar_font_size = 14.0f;
    theme->statusbar_height = 30.0f;

    /* Background */
    theme->background_mode = 1; /* shader by default */
    theme->background_shader[0] = '\0';
    theme->background_wallpaper[0] = '\0';

    /* Window transparency -- glass panes */
    theme->window_opacity = 0.95f;
    theme->inactive_opacity = 0.90f;

    VGP_LOG_INFO(TAG, "loaded default theme");
}

/* Simple key=value parser for theme files */
static int parse_hex_color(const char *str, vgp_color_t *color)
{
    /* Skip '#' if present */
    if (*str == '#') str++;

    unsigned long val = strtoul(str, NULL, 16);
    *color = vgp_color_hex((uint32_t)val);
    return 0;
}

static void trim(char *s)
{
    /* Trim leading */
    char *start = s;
    while (isspace(*start)) start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    /* Trim trailing */
    size_t len = strlen(s);
    while (len > 0 && isspace(s[len - 1]))
        s[--len] = '\0';
}

int vgp_theme_load(vgp_theme_t *theme, const char *path)
{
    /* Start with defaults */
    vgp_theme_load_defaults(theme);

    if (!path)
        return 0;

    FILE *f = fopen(path, "r");
    if (!f) {
        VGP_LOG_WARN(TAG, "cannot open theme file %s, using defaults", path);
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        trim(line);
        if (line[0] == '#' || line[0] == '\0' || line[0] == '[')
            continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);

        /* Strip quotes */
        size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
            val[vlen - 1] = '\0';
            val++;
        }

        /* Match known keys */
        if (strcmp(key, "titlebar_height") == 0)
            theme->titlebar_height = strtof(val, NULL);
        else if (strcmp(key, "border_width") == 0)
            theme->border_width = strtof(val, NULL);
        else if (strcmp(key, "corner_radius") == 0)
            theme->corner_radius = strtof(val, NULL);
        else if (strcmp(key, "titlebar_active") == 0)
            parse_hex_color(val, &theme->titlebar_active);
        else if (strcmp(key, "titlebar_inactive") == 0)
            parse_hex_color(val, &theme->titlebar_inactive);
        else if (strcmp(key, "border_active") == 0)
            parse_hex_color(val, &theme->border_active);
        else if (strcmp(key, "border_inactive") == 0)
            parse_hex_color(val, &theme->border_inactive);
        else if (strcmp(key, "background") == 0)
            parse_hex_color(val, &theme->background);
        else if (strcmp(key, "statusbar_bg") == 0)
            parse_hex_color(val, &theme->statusbar_bg);
        else if (strcmp(key, "statusbar_text") == 0)
            parse_hex_color(val, &theme->statusbar_text);
        else if (strcmp(key, "close_btn") == 0)
            parse_hex_color(val, &theme->close_btn);
        else if (strcmp(key, "maximize_btn") == 0)
            parse_hex_color(val, &theme->maximize_btn);
        else if (strcmp(key, "minimize_btn") == 0)
            parse_hex_color(val, &theme->minimize_btn);
        else if (strcmp(key, "statusbar_height") == 0)
            theme->statusbar_height = strtof(val, NULL);
    }

    fclose(f);
    VGP_LOG_INFO(TAG, "loaded theme from %s", path);
    return 0;
}
