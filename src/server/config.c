#include "config.h"
#include "vgp/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TAG "config"

void vgp_config_load_defaults(vgp_config_t *config)
{
    memset(config, 0, sizeof(*config));

    /* General */
    snprintf(config->general.terminal_cmd, sizeof(config->general.terminal_cmd),
             "vgp-term");
    snprintf(config->general.launcher_cmd, sizeof(config->general.launcher_cmd),
             "vgp-launcher");
    config->general.font_size = 14.0f;
    snprintf(config->general.wm_mode, sizeof(config->general.wm_mode), "floating");
    snprintf(config->general.tile_algorithm, sizeof(config->general.tile_algorithm), "golden_ratio");
    config->general.tile_master_ratio = 0.55f;
    config->general.tile_gap_inner = 6;
    config->general.tile_gap_outer = 8;
    config->general.tile_smart_gaps = true;
    snprintf(config->general.theme_name, sizeof(config->general.theme_name), "dark");

    /* Screenshot directory: XDG Pictures or HOME */
    {
        const char *home = getenv("HOME");
        const char *xdg_pics = getenv("XDG_PICTURES_DIR");
        if (xdg_pics)
            snprintf(config->general.screenshot_dir,
                     sizeof(config->general.screenshot_dir), "%s", xdg_pics);
        else if (home)
            snprintf(config->general.screenshot_dir,
                     sizeof(config->general.screenshot_dir), "%s/Pictures", home);
        else
            snprintf(config->general.screenshot_dir,
                     sizeof(config->general.screenshot_dir), "/tmp");
    }
    config->general.focus_follows_mouse = false;
    config->general.workspace_count = 9;

    /* Input */
    config->input.pointer_speed = 1.0f;
    config->input.natural_scrolling = false;
    config->input.tap_to_click = true;
    config->input.repeat_delay_ms = 300;
    config->input.repeat_rate_ms = 30;

    /* Theme */
    vgp_theme_load_defaults(&config->theme);

    /* Panel defaults */
    snprintf(config->panel.position, sizeof(config->panel.position), "bottom");
    config->panel.height = 32;
    /* Default widgets: left=workspaces, center=taskbar, right=clock */
    config->panel.left_count = 1;
    snprintf(config->panel.left_widgets[0], 32, "workspaces");
    config->panel.center_count = 1;
    snprintf(config->panel.center_widgets[0], 32, "taskbar");
    config->panel.right_count = 1;
    snprintf(config->panel.right_widgets[0], 32, "clock");

    /* Lock screen defaults */
    config->lockscreen.enabled = true;
    config->lockscreen.timeout_min = 5;

    /* Session defaults */
    config->session.autostart_count = 0;

    /* Window rules */
    config->window_rule_count = 0;

    /* Monitor defaults: auto layout, auto workspace */
    for (int i = 0; i < VGP_MAX_OUTPUTS; i++) {
        config->monitors[i].configured = false;
        config->monitors[i].x = -1;
        config->monitors[i].y = -1;
        config->monitors[i].workspace = -1;
        config->monitors[i].scale = 1.0f;
        config->monitors[i].mode[0] = '\0';
    }
    config->monitor_count = 0;

    /* Default keybinds */
    struct { const char *key; const char *action; } defaults[] = {
        { "Super+Return",         "spawn_terminal" },
        { "Super+d",              "spawn_launcher" },
        { "Super+q",              "close_window" },
        { "Super+f",              "fullscreen" },
        { "Super+m",              "maximize_window" },
        { "Super+n",              "minimize_window" },
        { "Alt+Tab",              "focus_next" },
        { "Alt+Shift+Tab",        "focus_prev" },
        { "Ctrl+Alt+BackSpace",   "quit" },
        { "Print",                "screenshot" },
        { "Super+1",              "workspace_1" },
        { "Super+2",              "workspace_2" },
        { "Super+3",              "workspace_3" },
        { "Super+Tab",            "expose" },
        { "Super+l",              "lock" },
        { "Super+space",          "toggle_float" },
        { "Super+Left",           "snap_left" },
        { "Super+Right",          "snap_right" },
        { "Super+Up",             "snap_top" },
        { "Super+Down",           "snap_bottom" },
        { "Super+Shift+1",        "move_to_workspace_1" },
        { "Super+Shift+2",        "move_to_workspace_2" },
        { "Super+Shift+3",        "move_to_workspace_3" },
        { NULL, NULL },
    };

    config->keybind_count = 0;
    for (int i = 0; defaults[i].key; i++) {
        if (config->keybind_count >= VGP_CONFIG_MAX_KEYBINDS) break;
        vgp_keybind_entry_t *e = &config->keybind_entries[config->keybind_count++];
        snprintf(e->key_str, sizeof(e->key_str), "%s", defaults[i].key);
        snprintf(e->action_str, sizeof(e->action_str), "%s", defaults[i].action);
    }
}

static void trim(char *s)
{
    char *start = s;
    while (isspace(*start)) start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace(s[len - 1]))
        s[--len] = '\0';
}

static void strip_quotes(char *val)
{
    size_t len = strlen(val);
    if (len >= 2 && val[0] == '"' && val[len - 1] == '"') {
        memmove(val, val + 1, len - 2);
        val[len - 2] = '\0';
    }
}

static int parse_hex_color(const char *str, vgp_color_t *color)
{
    if (*str == '#') str++;
    unsigned long v = strtoul(str, NULL, 16);
    *color = vgp_color_hex((uint32_t)v);
    return 0;
}

int vgp_config_load(vgp_config_t *config, const char *path)
{
    vgp_config_load_defaults(config);

    if (!path) {
        /* Resolve default path */
        const char *config_home = getenv("XDG_CONFIG_HOME");
        const char *home = getenv("HOME");
        if (config_home)
            snprintf(config->config_path, sizeof(config->config_path),
                     "%s/vgp/config.toml", config_home);
        else if (home)
            snprintf(config->config_path, sizeof(config->config_path),
                     "%s/.config/vgp/config.toml", home);
        else
            return 0; /* no config, use defaults */
        path = config->config_path;
    } else {
        snprintf(config->config_path, sizeof(config->config_path), "%s", path);
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        VGP_LOG_INFO(TAG, "no config at %s, using defaults", path);
        return 0;
    }

    char section[64] = "";
    char line[512];
    bool custom_keybinds = false;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;

        /* Section header */
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) {
                *end = '\0';
                snprintf(section, sizeof(section), "%s", line + 1);
                trim(section);
                if (strcmp(section, "keybinds") == 0 && !custom_keybinds) {
                    /* Clear defaults when user provides keybinds */
                    config->keybind_count = 0;
                    custom_keybinds = true;
                }
            }
            continue;
        }

        /* key = value */
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line;
        char *val = eq + 1;
        trim(key);
        trim(val);
        strip_quotes(val);

        if (strcmp(section, "general") == 0) {
            if (strcmp(key, "terminal") == 0)
                snprintf(config->general.terminal_cmd,
                         sizeof(config->general.terminal_cmd), "%s", val);
            else if (strcmp(key, "launcher") == 0)
                snprintf(config->general.launcher_cmd,
                         sizeof(config->general.launcher_cmd), "%s", val);
            else if (strcmp(key, "font") == 0)
                snprintf(config->general.font_path,
                         sizeof(config->general.font_path), "%s", val);
            else if (strcmp(key, "font_size") == 0)
                config->general.font_size = strtof(val, NULL);
            else if (strcmp(key, "focus_follows_mouse") == 0)
                config->general.focus_follows_mouse = strcmp(val, "true") == 0;
            else if (strcmp(key, "workspaces") == 0)
                config->general.workspace_count = atoi(val);
            else if (strcmp(key, "wm_mode") == 0)
                snprintf(config->general.wm_mode, sizeof(config->general.wm_mode), "%s", val);
            else if (strcmp(key, "tile_algorithm") == 0)
                snprintf(config->general.tile_algorithm, sizeof(config->general.tile_algorithm), "%s", val);
            else if (strcmp(key, "tile_master_ratio") == 0)
                config->general.tile_master_ratio = strtof(val, NULL);
            else if (strcmp(key, "tile_gap_inner") == 0)
                config->general.tile_gap_inner = atoi(val);
            else if (strcmp(key, "tile_gap_outer") == 0)
                config->general.tile_gap_outer = atoi(val);
            else if (strcmp(key, "tile_smart_gaps") == 0)
                config->general.tile_smart_gaps = strcmp(val, "true") == 0;
            else if (strcmp(key, "screenshot_dir") == 0)
                snprintf(config->general.screenshot_dir,
                         sizeof(config->general.screenshot_dir), "%s", val);
            else if (strcmp(key, "theme") == 0)
                snprintf(config->general.theme_name,
                         sizeof(config->general.theme_name), "%s", val);
        } else if (strcmp(section, "input") == 0) {
            if (strcmp(key, "pointer_speed") == 0)
                config->input.pointer_speed = strtof(val, NULL);
            else if (strcmp(key, "natural_scrolling") == 0)
                config->input.natural_scrolling = strcmp(val, "true") == 0;
            else if (strcmp(key, "tap_to_click") == 0)
                config->input.tap_to_click = strcmp(val, "true") == 0;
            else if (strcmp(key, "repeat_delay") == 0)
                config->input.repeat_delay_ms = atoi(val);
            else if (strcmp(key, "repeat_rate") == 0)
                config->input.repeat_rate_ms = atoi(val);
        } else if (strcmp(section, "keybinds") == 0) {
            if (config->keybind_count < VGP_CONFIG_MAX_KEYBINDS) {
                vgp_keybind_entry_t *e =
                    &config->keybind_entries[config->keybind_count++];
                snprintf(e->key_str, sizeof(e->key_str), "%s", key);
                snprintf(e->action_str, sizeof(e->action_str), "%s", val);
            }
        } else if (strcmp(section, "panel") == 0) {
            if (strcmp(key, "position") == 0)
                snprintf(config->panel.position, sizeof(config->panel.position), "%s", val);
            else if (strcmp(key, "height") == 0)
                config->panel.height = atoi(val);
        } else if (strcmp(section, "panel.widgets.left") == 0) {
            if (strcmp(key, "items") == 0) {
                /* Parse comma-separated list */
                config->panel.left_count = 0;
                char buf[256]; snprintf(buf, sizeof(buf), "%s", val);
                char *tok = strtok(buf, ",");
                while (tok && config->panel.left_count < VGP_PANEL_MAX_WIDGETS) {
                    while (*tok == ' ') tok++;
                    char *end = tok + strlen(tok) - 1;
                    while (end > tok && *end == ' ') *end-- = '\0';
                    snprintf(config->panel.left_widgets[config->panel.left_count++], 32, "%s", tok);
                    tok = strtok(NULL, ",");
                }
            }
        } else if (strcmp(section, "panel.widgets.center") == 0) {
            if (strcmp(key, "items") == 0) {
                config->panel.center_count = 0;
                char buf[256]; snprintf(buf, sizeof(buf), "%s", val);
                char *tok = strtok(buf, ",");
                while (tok && config->panel.center_count < VGP_PANEL_MAX_WIDGETS) {
                    while (*tok == ' ') tok++;
                    char *end = tok + strlen(tok) - 1;
                    while (end > tok && *end == ' ') *end-- = '\0';
                    snprintf(config->panel.center_widgets[config->panel.center_count++], 32, "%s", tok);
                    tok = strtok(NULL, ",");
                }
            }
        } else if (strcmp(section, "panel.widgets.right") == 0) {
            if (strcmp(key, "items") == 0) {
                config->panel.right_count = 0;
                char buf[256]; snprintf(buf, sizeof(buf), "%s", val);
                char *tok = strtok(buf, ",");
                while (tok && config->panel.right_count < VGP_PANEL_MAX_WIDGETS) {
                    while (*tok == ' ') tok++;
                    char *end = tok + strlen(tok) - 1;
                    while (end > tok && *end == ' ') *end-- = '\0';
                    snprintf(config->panel.right_widgets[config->panel.right_count++], 32, "%s", tok);
                    tok = strtok(NULL, ",");
                }
            }
        } else if (strncmp(section, "rule.", 5) == 0) {
            /* [rule.firefox], [rule.terminal], etc. */
            const char *rule_name = section + 5;
            int ri = -1;
            for (int i = 0; i < config->window_rule_count; i++) {
                if (strcmp(config->window_rules[i].title_match, rule_name) == 0) {
                    ri = i;
                    break;
                }
            }
            if (ri < 0 && config->window_rule_count < VGP_CONFIG_MAX_WINDOW_RULES) {
                ri = config->window_rule_count++;
                snprintf(config->window_rules[ri].title_match,
                         sizeof(config->window_rules[ri].title_match), "%s", rule_name);
                config->window_rules[ri].workspace = -1;
            }
            if (ri >= 0) {
                vgp_window_rule_t *r = &config->window_rules[ri];
                if (strcmp(key, "floating") == 0) r->floating = strcmp(val, "true") == 0;
                else if (strcmp(key, "workspace") == 0) r->workspace = atoi(val);
                else if (strcmp(key, "width") == 0) r->width = atoi(val);
                else if (strcmp(key, "height") == 0) r->height = atoi(val);
                else if (strcmp(key, "match") == 0)
                    snprintf(r->title_match, sizeof(r->title_match), "%s", val);
            }
        } else if (strcmp(section, "lockscreen") == 0) {
            if (strcmp(key, "enabled") == 0)
                config->lockscreen.enabled = strcmp(val, "true") == 0;
            else if (strcmp(key, "timeout") == 0)
                config->lockscreen.timeout_min = atoi(val);
        } else if (strcmp(section, "session") == 0) {
            if (strcmp(key, "autostart") == 0 &&
                config->session.autostart_count < VGP_CONFIG_MAX_AUTOSTART) {
                snprintf(config->session.autostart[config->session.autostart_count++],
                         256, "%s", val);
            }
        } else if (strncmp(section, "monitor.", 8) == 0) {
            /* [monitor.0], [monitor.1], etc. */
            int idx = atoi(section + 8);
            if (idx >= 0 && idx < VGP_MAX_OUTPUTS) {
                vgp_config_monitor_t *m = &config->monitors[idx];
                m->configured = true;
                if (idx >= config->monitor_count)
                    config->monitor_count = idx + 1;

                if (strcmp(key, "x") == 0) m->x = (int32_t)atoi(val);
                else if (strcmp(key, "y") == 0) m->y = (int32_t)atoi(val);
                else if (strcmp(key, "workspace") == 0) m->workspace = atoi(val);
                else if (strcmp(key, "scale") == 0) m->scale = strtof(val, NULL);
                else if (strcmp(key, "mode") == 0)
                    snprintf(m->mode, sizeof(m->mode), "%s", val);
            }
        } else if (strcmp(section, "theme") == 0) {
            /* Reuse theme parsing */
            vgp_theme_t *t = &config->theme;
            if (strcmp(key, "titlebar_height") == 0)
                t->titlebar_height = strtof(val, NULL);
            else if (strcmp(key, "border_width") == 0)
                t->border_width = strtof(val, NULL);
            else if (strcmp(key, "corner_radius") == 0)
                t->corner_radius = strtof(val, NULL);
            else if (strcmp(key, "statusbar_height") == 0)
                t->statusbar_height = strtof(val, NULL);
            else if (strcmp(key, "titlebar_active") == 0)
                parse_hex_color(val, &t->titlebar_active);
            else if (strcmp(key, "titlebar_inactive") == 0)
                parse_hex_color(val, &t->titlebar_inactive);
            else if (strcmp(key, "border_active") == 0)
                parse_hex_color(val, &t->border_active);
            else if (strcmp(key, "border_inactive") == 0)
                parse_hex_color(val, &t->border_inactive);
            else if (strcmp(key, "background") == 0)
                parse_hex_color(val, &t->background);
            else if (strcmp(key, "statusbar_bg") == 0)
                parse_hex_color(val, &t->statusbar_bg);
            else if (strcmp(key, "statusbar_text") == 0)
                parse_hex_color(val, &t->statusbar_text);
            else if (strcmp(key, "close_btn") == 0)
                parse_hex_color(val, &t->close_btn);
            else if (strcmp(key, "maximize_btn") == 0)
                parse_hex_color(val, &t->maximize_btn);
            else if (strcmp(key, "minimize_btn") == 0)
                parse_hex_color(val, &t->minimize_btn);
        }
    }

    fclose(f);

    /* Resolve and load theme directory */
    {
        const char *home = getenv("HOME");
        char theme_path[VGP_CONFIG_MAX_PATH];

        /* Try: ~/.config/vgp/themes/<name>/theme.toml */
        if (home) {
            snprintf(theme_path, sizeof(theme_path),
                     "%s/.config/vgp/themes/%s/theme.toml",
                     home, config->general.theme_name);
            snprintf(config->general.theme_dir, sizeof(config->general.theme_dir),
                     "%s/.config/vgp/themes/%s",
                     home, config->general.theme_name);
        } else {
            theme_path[0] = '\0';
        }

        FILE *tf = fopen(theme_path, "r");
        if (tf) {
            VGP_LOG_INFO(TAG, "loading theme '%s' from %s",
                         config->general.theme_name, theme_path);
            char tline[512], tsection[64] = "";
            vgp_theme_t *t = &config->theme;

            while (fgets(tline, sizeof(tline), tf)) {
                trim(tline);
                if (tline[0] == '#' || tline[0] == '\0') continue;
                if (tline[0] == '[') {
                    char *end = strchr(tline, ']');
                    if (end) { *end = '\0'; snprintf(tsection, sizeof(tsection), "%s", tline + 1); }
                    continue;
                }
                char *teq = strchr(tline, '=');
                if (!teq) continue;
                *teq = '\0';
                char *tk = tline, *tv = teq + 1;
                trim(tk); trim(tv); strip_quotes(tv);

                if (strcmp(tsection, "colors") == 0) {
                    if (strcmp(tk, "background") == 0) parse_hex_color(tv, &t->background);
                    else if (strcmp(tk, "accent") == 0) parse_hex_color(tv, &t->border_active);
                    else if (strcmp(tk, "surface") == 0) parse_hex_color(tv, &t->content_bg);
                    else if (strcmp(tk, "border") == 0) parse_hex_color(tv, &t->border_inactive);
                    else if (strcmp(tk, "error") == 0) parse_hex_color(tv, &t->close_btn);
                    else if (strcmp(tk, "success") == 0) parse_hex_color(tv, &t->maximize_btn);
                    else if (strcmp(tk, "warning") == 0) parse_hex_color(tv, &t->minimize_btn);
                } else if (strcmp(tsection, "window") == 0) {
                    if (strcmp(tk, "titlebar_height") == 0) t->titlebar_height = strtof(tv, NULL);
                    else if (strcmp(tk, "border_width") == 0) t->border_width = strtof(tv, NULL);
                    else if (strcmp(tk, "corner_radius") == 0) t->corner_radius = strtof(tv, NULL);
                } else if (strcmp(tsection, "window.active") == 0) {
                    if (strcmp(tk, "titlebar_bg") == 0) parse_hex_color(tv, &t->titlebar_active);
                    else if (strcmp(tk, "titlebar_text") == 0) parse_hex_color(tv, &t->title_text_active);
                    else if (strcmp(tk, "border_color") == 0) parse_hex_color(tv, &t->border_active);
                } else if (strcmp(tsection, "window.inactive") == 0) {
                    if (strcmp(tk, "titlebar_bg") == 0) parse_hex_color(tv, &t->titlebar_inactive);
                    else if (strcmp(tk, "titlebar_text") == 0) parse_hex_color(tv, &t->title_text_inactive);
                    else if (strcmp(tk, "border_color") == 0) parse_hex_color(tv, &t->border_inactive);
                } else if (strcmp(tsection, "panel") == 0) {
                    if (strcmp(tk, "bg") == 0) parse_hex_color(tv, &t->statusbar_bg);
                    else if (strcmp(tk, "text") == 0) parse_hex_color(tv, &t->statusbar_text);
                    else if (strcmp(tk, "height") == 0) t->statusbar_height = strtof(tv, NULL);
                } else if (strcmp(tsection, "fonts") == 0) {
                    if (strcmp(tk, "ui_size") == 0) t->statusbar_font_size = strtof(tv, NULL);
                    else if (strcmp(tk, "title_size") == 0) t->title_font_size = strtof(tv, NULL);
                } else if (strcmp(tsection, "shaders") == 0) {
                    if (strcmp(tk, "background") == 0) {
                        snprintf(t->background_shader, sizeof(t->background_shader),
                                 "%s/%s", config->general.theme_dir, tv);
                        t->background_mode = 1;
                    }
                } else if (strcmp(tsection, "background") == 0) {
                    if (strcmp(tk, "mode") == 0) {
                        if (strcmp(tv, "solid") == 0) t->background_mode = 0;
                        else if (strcmp(tv, "shader") == 0) t->background_mode = 1;
                        else if (strcmp(tv, "wallpaper") == 0) t->background_mode = 2;
                        else if (strcmp(tv, "none") == 0) t->background_mode = 3;
                    }
                    else if (strcmp(tk, "wallpaper") == 0)
                        snprintf(t->background_wallpaper, sizeof(t->background_wallpaper), "%s", tv);
                } else if (strcmp(tsection, "window.buttons") == 0) {
                    if (strcmp(tk, "radius") == 0) t->button_radius = strtof(tv, NULL);
                    else if (strcmp(tk, "spacing") == 0) t->button_spacing = strtof(tv, NULL);
                    else if (strcmp(tk, "margin") == 0) t->button_margin_right = strtof(tv, NULL);
                    else if (strcmp(tk, "close") == 0) parse_hex_color(tv, &t->close_btn);
                    else if (strcmp(tk, "maximize") == 0) parse_hex_color(tv, &t->maximize_btn);
                    else if (strcmp(tk, "minimize") == 0) parse_hex_color(tv, &t->minimize_btn);
                }

                /* Window opacity */
                if (strcmp(tsection, "window") == 0) {
                    if (strcmp(tk, "opacity") == 0) t->window_opacity = strtof(tv, NULL);
                    else if (strcmp(tk, "inactive_opacity") == 0) t->inactive_opacity = strtof(tv, NULL);
                }
            }
            fclose(tf);
        } else {
            VGP_LOG_INFO(TAG, "theme '%s' not found, using defaults",
                         config->general.theme_name);
        }
    }

    VGP_LOG_INFO(TAG, "loaded config from %s (%d keybinds, theme=%s)",
                 path, config->keybind_count, config->general.theme_name);
    return 0;
}
