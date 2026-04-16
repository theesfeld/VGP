/* VGP Settings -- Full GUI configuration editor
 * Every setting is editable with proper widgets.
 * Changes write back to config.toml. */

#include "vgp-ui.h"
#include "config-writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

/* ============================================================
 * Application state
 * ============================================================ */

typedef enum {
    TAB_GENERAL,
    TAB_PANEL,
    TAB_THEME,
    TAB_KEYBINDS,
    TAB_BACKGROUND,
    TAB_MONITORS,
    TAB_LOCKSCREEN,
    TAB_ACCESSIBILITY,
    TAB_ABOUT,
    TAB_COUNT,
} settings_tab_t;

static const char *tab_names[] = {
    "General", "Panel", "Theme", "Keybinds", "Background",
    "Monitors", "Lock", "Access", "About"
};

typedef struct {
    settings_tab_t  current_tab;
    int             edit_field;

    /* General */
    char            terminal[256];
    char            launcher[256];
    char            screenshot_dir[256];
    int             workspaces;
    float           pointer_speed;

    /* Panel */
    int             panel_position;  /* 0=bottom, 1=top */
    int             panel_height;
    char            panel_left[256];
    char            panel_center[256];
    char            panel_right[256];

    /* WM */
    int             wm_mode;   /* 0=floating, 1=tiling, 2=hybrid */
    int             tile_algo; /* 0=golden_ratio, 1=equal, 2=master_stack, 3=spiral */
    float           tile_master_ratio;
    int             tile_gap_inner;
    int             tile_gap_outer;
    bool            tile_smart_gaps;

    /* Background */
    int             bg_mode;  /* 0=solid, 1=shader, 2=wallpaper, 3=none */
    char            bg_shader[256];
    char            bg_wallpaper[256];

    /* Lock screen */
    bool            lock_enabled;
    int             lock_timeout;

    /* Accessibility */
    bool            a11y_high_contrast;
    bool            a11y_focus_indicator;
    float           a11y_font_scale;
    bool            a11y_reduce_anims;
    bool            a11y_large_cursor;

    /* Theme */
    char            theme_name[64];
    char            themes[32][64];
    int             theme_count;

    /* Keybinds */
    struct { char key[64]; char action[256]; bool capturing; } keybinds[128];
    int             keybind_count;

    /* Dropdown open states */
    bool            dd_panel_pos;
    bool            dd_wm_mode;
    bool            dd_tile_algo;
    bool            dd_bg_mode;

    char            status[128];
    int             status_timer;
    char            config_path[512];
} settings_state_t;

static settings_state_t state;

/* ============================================================
 * Config loading / saving
 * ============================================================ */

static void scan_themes(void)
{
    state.theme_count = 0;
    const char *home = getenv("HOME");
    if (!home) return;
    char path[512];
    snprintf(path, sizeof(path), "%s/.config/vgp/themes", home);
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && state.theme_count < 32) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type != DT_DIR) continue;
        snprintf(state.themes[state.theme_count++], 64, "%s", entry->d_name);
    }
    closedir(dir);
}

static void load_config(void)
{
    const char *home = getenv("HOME");
    if (!home) return;
    snprintf(state.config_path, sizeof(state.config_path),
             "%s/.config/vgp/config.toml", home);

    /* Defaults */
    snprintf(state.terminal, sizeof(state.terminal), "vgp-term");
    snprintf(state.launcher, sizeof(state.launcher), "vgp-launcher");
    snprintf(state.theme_name, sizeof(state.theme_name), "dark");
    snprintf(state.screenshot_dir, sizeof(state.screenshot_dir), "%s/Pictures", home);
    state.workspaces = 9;
    state.pointer_speed = 3.0f;
    state.panel_position = 0;
    state.panel_height = 32;
    snprintf(state.panel_left, sizeof(state.panel_left), "workspaces");
    snprintf(state.panel_center, sizeof(state.panel_center), "taskbar");
    snprintf(state.panel_right, sizeof(state.panel_right), "clock, date");
    state.wm_mode = 0;
    state.tile_algo = 0;
    state.tile_master_ratio = 0.55f;
    state.tile_gap_inner = 6;
    state.tile_gap_outer = 8;
    state.tile_smart_gaps = true;
    state.bg_mode = 1;
    state.lock_enabled = true;
    state.lock_timeout = 5;
    state.a11y_font_scale = 1.0f;
    state.edit_field = -1;

    FILE *f = fopen(state.config_path, "r");
    if (!f) return;

    char line[512], section[64] = "";
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' '))
            line[--len] = '\0';
        char *s = line;
        while (*s == ' ') s++;
        if (s[0] == '#' || s[0] == '\0') continue;
        if (s[0] == '[') {
            char *end = strchr(s, ']');
            if (end) { *end = '\0'; snprintf(section, sizeof(section), "%s", s + 1); }
            continue;
        }
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = s, *val = eq + 1;
        while (*key == ' ') key++;
        len = strlen(key); while (len > 0 && key[len-1] == ' ') key[--len] = '\0';
        while (*val == ' ') val++;
        len = strlen(val);
        if (len >= 2 && val[0] == '"' && val[len-1] == '"') { val[len-1] = '\0'; val++; }

        if (strcmp(section, "general") == 0) {
            if (strcmp(key, "terminal") == 0) snprintf(state.terminal, sizeof(state.terminal), "%s", val);
            else if (strcmp(key, "launcher") == 0) snprintf(state.launcher, sizeof(state.launcher), "%s", val);
            else if (strcmp(key, "theme") == 0) snprintf(state.theme_name, sizeof(state.theme_name), "%s", val);
            else if (strcmp(key, "workspaces") == 0) state.workspaces = atoi(val);
            else if (strcmp(key, "screenshot_dir") == 0) snprintf(state.screenshot_dir, sizeof(state.screenshot_dir), "%s", val);
            else if (strcmp(key, "wm_mode") == 0) {
                if (strcmp(val, "tiling") == 0) state.wm_mode = 1;
                else if (strcmp(val, "hybrid") == 0) state.wm_mode = 2;
                else state.wm_mode = 0;
            }
            else if (strcmp(key, "tile_algorithm") == 0) {
                if (strcmp(val, "equal") == 0) state.tile_algo = 1;
                else if (strcmp(val, "master_stack") == 0) state.tile_algo = 2;
                else if (strcmp(val, "spiral") == 0) state.tile_algo = 3;
                else state.tile_algo = 0;
            }
            else if (strcmp(key, "tile_master_ratio") == 0) state.tile_master_ratio = (float)atof(val);
            else if (strcmp(key, "tile_gap_inner") == 0) state.tile_gap_inner = atoi(val);
            else if (strcmp(key, "tile_gap_outer") == 0) state.tile_gap_outer = atoi(val);
            else if (strcmp(key, "tile_smart_gaps") == 0) state.tile_smart_gaps = strcmp(val, "true") == 0;
        } else if (strcmp(section, "input") == 0) {
            if (strcmp(key, "pointer_speed") == 0) state.pointer_speed = (float)atof(val);
        } else if (strcmp(section, "panel") == 0) {
            if (strcmp(key, "position") == 0) state.panel_position = strcmp(val, "top") == 0 ? 1 : 0;
            else if (strcmp(key, "height") == 0) state.panel_height = atoi(val);
        } else if (strcmp(section, "panel.widgets.left") == 0) {
            if (strcmp(key, "items") == 0) snprintf(state.panel_left, sizeof(state.panel_left), "%s", val);
        } else if (strcmp(section, "panel.widgets.center") == 0) {
            if (strcmp(key, "items") == 0) snprintf(state.panel_center, sizeof(state.panel_center), "%s", val);
        } else if (strcmp(section, "panel.widgets.right") == 0) {
            if (strcmp(key, "items") == 0) snprintf(state.panel_right, sizeof(state.panel_right), "%s", val);
        } else if (strcmp(section, "keybinds") == 0) {
            if (state.keybind_count < 128) {
                snprintf(state.keybinds[state.keybind_count].key, 64, "%s", key);
                snprintf(state.keybinds[state.keybind_count].action, 256, "%s", val);
                state.keybinds[state.keybind_count].capturing = false;
                state.keybind_count++;
            }
        } else if (strcmp(section, "lockscreen") == 0) {
            if (strcmp(key, "enabled") == 0) state.lock_enabled = strcmp(val, "true") == 0;
            else if (strcmp(key, "timeout") == 0) state.lock_timeout = atoi(val);
        } else if (strcmp(section, "background") == 0) {
            if (strcmp(key, "mode") == 0) {
                if (strcmp(val, "shader") == 0) state.bg_mode = 1;
                else if (strcmp(val, "wallpaper") == 0) state.bg_mode = 2;
                else if (strcmp(val, "none") == 0) state.bg_mode = 3;
                else state.bg_mode = 0;
            }
            else if (strcmp(key, "shader") == 0) snprintf(state.bg_shader, sizeof(state.bg_shader), "%s", val);
            else if (strcmp(key, "wallpaper") == 0) snprintf(state.bg_wallpaper, sizeof(state.bg_wallpaper), "%s", val);
        } else if (strcmp(section, "accessibility") == 0) {
            if (strcmp(key, "high_contrast") == 0) state.a11y_high_contrast = strcmp(val, "true") == 0;
            else if (strcmp(key, "focus_indicator") == 0) state.a11y_focus_indicator = strcmp(val, "true") == 0;
            else if (strcmp(key, "font_scale") == 0) state.a11y_font_scale = (float)atof(val);
            else if (strcmp(key, "reduce_animations") == 0) state.a11y_reduce_anims = strcmp(val, "true") == 0;
            else if (strcmp(key, "large_cursor") == 0) state.a11y_large_cursor = strcmp(val, "true") == 0;
        }
    }
    fclose(f);
}

static const char *wm_modes[] = {"floating", "tiling", "hybrid"};
static const char *tile_algos[] = {"golden_ratio", "equal", "master_stack", "spiral"};
static const char *panel_positions[] = {"bottom", "top"};
static const char *bg_modes[] = {"solid", "shader", "wallpaper", "none"};

static void save_config(void)
{
    const char *p = state.config_path;
    config_set_value(p, "general", "terminal", state.terminal);
    config_set_value(p, "general", "launcher", state.launcher);
    config_set_value(p, "general", "theme", state.theme_name);
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", state.workspaces);
    config_set_value(p, "general", "workspaces", buf);
    config_set_value(p, "general", "screenshot_dir", state.screenshot_dir);
    config_set_value(p, "general", "wm_mode", wm_modes[state.wm_mode]);
    config_set_value(p, "general", "tile_algorithm", tile_algos[state.tile_algo]);
    snprintf(buf, sizeof(buf), "%.2f", state.tile_master_ratio);
    config_set_value(p, "general", "tile_master_ratio", buf);
    snprintf(buf, sizeof(buf), "%d", state.tile_gap_inner);
    config_set_value(p, "general", "tile_gap_inner", buf);
    snprintf(buf, sizeof(buf), "%d", state.tile_gap_outer);
    config_set_value(p, "general", "tile_gap_outer", buf);
    config_set_value(p, "general", "tile_smart_gaps", state.tile_smart_gaps ? "true" : "false");
    snprintf(buf, sizeof(buf), "%.1f", state.pointer_speed);
    config_set_value(p, "input", "pointer_speed", buf);
    config_set_value(p, "panel", "position", panel_positions[state.panel_position]);
    snprintf(buf, sizeof(buf), "%d", state.panel_height);
    config_set_value(p, "panel", "height", buf);
    config_set_value(p, "panel.widgets.left", "items", state.panel_left);
    config_set_value(p, "panel.widgets.center", "items", state.panel_center);
    config_set_value(p, "panel.widgets.right", "items", state.panel_right);
    config_set_value(p, "lockscreen", "enabled", state.lock_enabled ? "true" : "false");
    snprintf(buf, sizeof(buf), "%d", state.lock_timeout);
    config_set_value(p, "lockscreen", "timeout", buf);
    config_set_value(p, "accessibility", "high_contrast", state.a11y_high_contrast ? "true" : "false");
    config_set_value(p, "accessibility", "focus_indicator", state.a11y_focus_indicator ? "true" : "false");
    snprintf(buf, sizeof(buf), "%.1f", state.a11y_font_scale);
    config_set_value(p, "accessibility", "font_scale", buf);
    config_set_value(p, "accessibility", "reduce_animations", state.a11y_reduce_anims ? "true" : "false");
    config_set_value(p, "accessibility", "large_cursor", state.a11y_large_cursor ? "true" : "false");

    snprintf(state.status, sizeof(state.status), "Settings saved! Reload with SIGHUP or restart.");
    state.status_timer = 180;
}

static void set_status(const char *msg)
{
    snprintf(state.status, sizeof(state.status), "%s", msg);
    state.status_timer = 120;
}

/* ============================================================
 * Editable field (for text inputs only)
 * ============================================================ */

static bool editable_field(vui_ctx_t *ctx, int row, int col, int label_w,
                            const char *label, char *buffer, int buf_size,
                            int field_id, int field_width)
{
    vui_label(ctx, row, col, label, VUI_GRAY);
    bool editing = (state.edit_field == field_id);
    vui_color_t bg = editing ? VUI_SURFACE : VUI_BG;
    vui_color_t fg = editing ? VUI_WHITE : VUI_ACCENT;

    vui_fill(ctx, row, col + label_w, 1, field_width, bg);
    vui_text(ctx, row, col + label_w + 1, buffer, fg, bg);

    if (editing) {
        int cpos = (int)strlen(buffer);
        if (cpos < field_width - 2)
            vui_set_cell(ctx, row, col + label_w + 1 + cpos, '_', VUI_ACCENT, bg, VGP_CELL_BLINK);
    }

    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= col + label_w &&
                   ctx->mouse_col < col + label_w + field_width);
    if (hover && ctx->mouse_clicked) {
        state.edit_field = field_id;
        return false;
    }

    if (editing && ctx->key_pressed) {
        int len = (int)strlen(buffer);
        if (ctx->last_keysym == 0xFF08 && len > 0) buffer[len - 1] = '\0';
        else if (ctx->last_keysym == 0xFF0D || ctx->last_keysym == 0xFF09) { state.edit_field = -1; return true; }
        else if (ctx->last_keysym == 0xFF1B) state.edit_field = -1;
        else if (ctx->last_utf8[0] >= 0x20 && len < buf_size - 1) { buffer[len] = ctx->last_utf8[0]; buffer[len + 1] = '\0'; }
    }
    return false;
}

/* ============================================================
 * Tab renderers
 * ============================================================ */

static void render_general(vui_ctx_t *ctx, int y, int x, int w)
{
    int fw = w - 24;
    vui_section(ctx, y++, x, w, "General", VUI_ACCENT); y++;
    if (editable_field(ctx, y, x+2, 18, "Terminal:", state.terminal, 256, 1, fw)) set_status("Terminal changed");
    vui_tooltip(ctx, y++, x+2, 18, "Default terminal emulator command");
    y++;
    if (editable_field(ctx, y, x+2, 18, "Launcher:", state.launcher, 256, 2, fw)) set_status("Launcher changed");
    vui_tooltip(ctx, y++, x+2, 18, "Application launcher command");
    y++;
    if (editable_field(ctx, y, x+2, 18, "Screenshot dir:", state.screenshot_dir, 256, 4, fw)) set_status("Screenshot dir changed");
    vui_tooltip(ctx, y++, x+2, 18, "Directory for screenshots (Print key)");
    y++;

    vui_label(ctx, y, x+2, "Workspaces:", VUI_GRAY);
    float ws_f = (float)state.workspaces;
    if (vui_slider(ctx, y, x + 18, 30, &ws_f, 1, 9, "%.0f")) {
        state.workspaces = (int)ws_f;
        set_status("Workspaces changed");
    }
    y += 2;

    vui_section(ctx, y++, x, w, "Window Management", VUI_ACCENT); y++;

    vui_label(ctx, y, x+2, "WM Mode:", VUI_GRAY);
    if (vui_dropdown(ctx, y, x + 18, 20, wm_modes, 3, &state.wm_mode, &state.dd_wm_mode))
        set_status("WM mode changed");
    vui_tooltip(ctx, y, x+2, 16, "floating=free, tiling=auto-tile, hybrid=both");
    if (state.dd_wm_mode) { y += 5; } else { y += 2; }

    if (state.wm_mode >= 1) {
        vui_label(ctx, y, x+2, "Tile algorithm:", VUI_GRAY);
        if (vui_dropdown(ctx, y, x + 18, 20, tile_algos, 4, &state.tile_algo, &state.dd_tile_algo))
            set_status("Tile algorithm changed");
        if (state.dd_tile_algo) { y += 6; } else { y += 2; }

        vui_label(ctx, y, x+2, "Master ratio:", VUI_GRAY);
        if (vui_slider(ctx, y, x + 18, 30, &state.tile_master_ratio, 0.2f, 0.8f, "%.2f"))
            set_status("Master ratio changed");
        y += 2;

        float gap_i = (float)state.tile_gap_inner;
        vui_label(ctx, y, x+2, "Gap inner:", VUI_GRAY);
        if (vui_slider(ctx, y, x + 18, 30, &gap_i, 0, 20, "%.0f px")) {
            state.tile_gap_inner = (int)gap_i;
            set_status("Inner gap changed");
        }
        y += 1;
        float gap_o = (float)state.tile_gap_outer;
        vui_label(ctx, y, x+2, "Gap outer:", VUI_GRAY);
        if (vui_slider(ctx, y, x + 18, 30, &gap_o, 0, 20, "%.0f px")) {
            state.tile_gap_outer = (int)gap_o;
            set_status("Outer gap changed");
        }
        y += 2;

        if (vui_checkbox(ctx, y, x+2, "Smart gaps (hide when 1 window)", &state.tile_smart_gaps))
            set_status("Smart gaps toggled");
        y += 2;
    }

    vui_section(ctx, y++, x, w, "Input", VUI_ACCENT); y++;
    vui_label(ctx, y, x+2, "Pointer speed:", VUI_GRAY);
    if (vui_slider(ctx, y, x + 18, 30, &state.pointer_speed, 0.5f, 10.0f, "%.1f"))
        set_status("Pointer speed changed");
    y += 2;

    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_panel(vui_ctx_t *ctx, int y, int x, int w)
{
    int fw = w - 24;
    vui_section(ctx, y++, x, w, "Panel", VUI_ACCENT); y++;

    vui_label(ctx, y, x+2, "Position:", VUI_GRAY);
    if (vui_dropdown(ctx, y, x + 18, 16, panel_positions, 2, &state.panel_position, &state.dd_panel_pos))
        set_status("Panel position changed");
    if (state.dd_panel_pos) { y += 4; } else { y += 2; }

    float h_f = (float)state.panel_height;
    vui_label(ctx, y, x+2, "Height:", VUI_GRAY);
    if (vui_slider(ctx, y, x + 18, 30, &h_f, 20, 48, "%.0f px")) {
        state.panel_height = (int)h_f;
        set_status("Panel height changed");
    }
    y += 3;

    vui_section(ctx, y++, x, w, "Widget Placement (comma-separated)", VUI_ACCENT); y++;
    if (editable_field(ctx, y, x+2, 12, "Left:", state.panel_left, 256, 12, fw)) set_status("Left widgets changed");
    vui_tooltip(ctx, y++, x+2, 12, "Widgets anchored to the left side");
    y++;
    if (editable_field(ctx, y, x+2, 12, "Center:", state.panel_center, 256, 13, fw)) set_status("Center widgets changed");
    vui_tooltip(ctx, y++, x+2, 12, "Widgets in the center (taskbar fills available space)");
    y++;
    if (editable_field(ctx, y, x+2, 12, "Right:", state.panel_right, 256, 14, fw)) set_status("Right widgets changed");
    vui_tooltip(ctx, y++, x+2, 12, "Widgets anchored to the right side");
    y += 2;

    vui_section(ctx, y++, x, w, "Available Widgets", VUI_GRAY); y++;
    const char *widgets[][2] = {
        {"workspaces", "Numbered workspace indicators"},
        {"taskbar",    "Window list for current workspace"},
        {"clock",      "Current time (HH:MM)"},
        {"date",       "Current date (MM/DD)"},
        {"cpu",        "CPU usage percentage"},
        {"memory",     "Memory usage percentage"},
        {"battery",    "Battery level + charging state"},
    };
    for (int i = 0; i < 7; i++) {
        vui_text_bold(ctx, y, x + 4, widgets[i][0], VUI_ACCENT, VUI_BG);
        vui_label(ctx, y, x + 18, widgets[i][1], VUI_GRAY);
        y++;
    }
    y += 2;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_themes(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Theme", VUI_ACCENT); y++;
    vui_label(ctx, y, x+2, "Active:", VUI_GRAY);
    vui_text_bold(ctx, y++, x + 14, state.theme_name, VUI_ACCENT, VUI_BG); y++;
    vui_label(ctx, y++, x+2, "Click a theme to switch:", VUI_GRAY); y++;
    for (int i = 0; i < state.theme_count; i++) {
        bool sel = (strcmp(state.themes[i], state.theme_name) == 0);
        if (vui_list_item(ctx, y + i, x + 2, w - 4, state.themes[i], sel, false)) {
            snprintf(state.theme_name, sizeof(state.theme_name), "%s", state.themes[i]);
            config_set_value(state.config_path, "general", "theme", state.theme_name);
            set_status("Theme changed (hot-reloaded)");
        }
    }
}

static void render_keybinds(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Keybinds", VUI_ACCENT); y++;
    vui_text_bold(ctx, y, x+2, "Key Combo", VUI_GRAY, VUI_BG);
    vui_text_bold(ctx, y++, x+28, "Action", VUI_GRAY, VUI_BG);
    vui_hline(ctx, y++, x+2, w-4, VUI_BORDER, VUI_BG);

    int vis = ctx->rows - y - 3;
    for (int i = ctx->scroll_offset; i < state.keybind_count && (i - ctx->scroll_offset) < vis; i++) {
        int row = y + (i - ctx->scroll_offset);
        /* Keybind capture field */
        if (vui_keybind_input(ctx, row, x + 2, 24, state.keybinds[i].key, 64,
                               &state.keybinds[i].capturing)) {
            set_status("Keybind changed (save to apply)");
        }
        /* Action */
        vui_text(ctx, row, x + 28, state.keybinds[i].action, VUI_ACCENT, VUI_BG);
    }

    /* Scrollbar */
    if (state.keybind_count > vis) {
        vui_scrollbar(ctx, y, x + w - 2, vis, vis, state.keybind_count, ctx->scroll_offset);
    }
}

static void render_background(vui_ctx_t *ctx, int y, int x, int w)
{
    int fw = w - 24;
    vui_section(ctx, y++, x, w, "Background", VUI_ACCENT); y++;

    vui_label(ctx, y, x+2, "Mode:", VUI_GRAY);
    if (vui_dropdown(ctx, y, x + 18, 20, bg_modes, 4, &state.bg_mode, &state.dd_bg_mode))
        set_status("Background mode changed");
    if (state.dd_bg_mode) { y += 6; } else { y += 2; }

    if (state.bg_mode == 1) {
        if (editable_field(ctx, y, x+2, 18, "Shader path:", state.bg_shader, 256, 21, fw))
            set_status("Shader changed");
        vui_tooltip(ctx, y, x+2, 18, "Path relative to theme dir or ~/.config/vgp/shaders/");
        y += 2;
    } else if (state.bg_mode == 2) {
        if (editable_field(ctx, y, x+2, 18, "Wallpaper:", state.bg_wallpaper, 256, 22, fw))
            set_status("Wallpaper changed");
        y += 2;
    }

    y++;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_monitors(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Monitors", VUI_ACCENT); y++;
    vui_label(ctx, y++, x+2, "Per-monitor configuration via config.toml [monitor.N] sections.", VUI_GRAY); y++;
    vui_label(ctx, y++, x+4, "x, y       = position in global layout", VUI_GRAY);
    vui_label(ctx, y++, x+4, "workspace  = which workspace to display", VUI_GRAY);
    y++;
    vui_label(ctx, y++, x+2, "Example:", VUI_GRAY); y++;
    vui_text(ctx, y++, x+4, "[monitor.0]", VUI_ACCENT, VUI_BG);
    vui_text(ctx, y++, x+4, "x = 0", VUI_WHITE, VUI_BG);
    vui_text(ctx, y++, x+4, "y = 0", VUI_WHITE, VUI_BG);
    vui_text(ctx, y++, x+4, "workspace = 0", VUI_WHITE, VUI_BG);
}

static void render_lockscreen(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Lock Screen", VUI_ACCENT); y++;
    if (vui_checkbox(ctx, y, x+2, "Enable lock screen", &state.lock_enabled))
        set_status("Lock screen toggled");
    vui_tooltip(ctx, y, x+2, 30, "Lock screen activates after idle timeout");
    y += 2;

    if (state.lock_enabled) {
        float timeout_f = (float)state.lock_timeout;
        vui_label(ctx, y, x+2, "Idle timeout:", VUI_GRAY);
        if (vui_slider(ctx, y, x + 18, 30, &timeout_f, 1, 30, "%.0f min")) {
            state.lock_timeout = (int)timeout_f;
            set_status("Lock timeout changed");
        }
        y += 2;
    }

    vui_label(ctx, y++, x+2, "Authentication via PAM. Uses theme shader background.", VUI_GRAY);
    y += 2;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_accessibility(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Accessibility", VUI_ACCENT); y++;

    if (vui_checkbox(ctx, y, x+2, "High contrast mode", &state.a11y_high_contrast))
        set_status("High contrast toggled");
    vui_tooltip(ctx, y, x+2, 30, "Override theme with high-contrast colors");
    y += 2;

    if (vui_checkbox(ctx, y, x+2, "Focus indicator ring", &state.a11y_focus_indicator))
        set_status("Focus indicator toggled");
    vui_tooltip(ctx, y, x+2, 30, "Bright yellow ring around focused window");
    y += 2;

    if (vui_checkbox(ctx, y, x+2, "Reduce animations", &state.a11y_reduce_anims))
        set_status("Reduce animations toggled");
    vui_tooltip(ctx, y, x+2, 30, "Disable window open/close/slide animations");
    y += 2;

    if (vui_checkbox(ctx, y, x+2, "Large cursor", &state.a11y_large_cursor))
        set_status("Large cursor toggled");
    vui_tooltip(ctx, y, x+2, 30, "Double-size cursor for visibility");
    y += 2;

    vui_label(ctx, y, x+2, "Font scale:", VUI_GRAY);
    if (vui_slider(ctx, y, x + 18, 30, &state.a11y_font_scale, 0.5f, 3.0f, "%.1fx"))
        set_status("Font scale changed");
    y += 3;

    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_about(vui_ctx_t *ctx, int y, int x, int w)
{
    (void)w;
    vui_text_bold(ctx, y++, x+2, "VGP - Vector Graphics Protocol", VUI_ACCENT, VUI_BG); y++;
    vui_label(ctx, y++, x+2, "Version 0.1.0", VUI_WHITE);
    vui_label(ctx, y++, x+2, "GPU-accelerated vector display server for Linux", VUI_GRAY); y++;
    vui_label(ctx, y++, x+2, "All rendering is vector-based. No X11. No Wayland.", VUI_GRAY); y++;
    vui_label(ctx, y++, x+2, "https://github.com/theesfeld/VGP", VUI_ACCENT);
    y += 2;
    vui_label(ctx, y++, x+2, "Keybinds:", VUI_WHITE);
    vui_label(ctx, y++, x+4, "Super+Return     Open terminal", VUI_GRAY);
    vui_label(ctx, y++, x+4, "Super+D          Open launcher", VUI_GRAY);
    vui_label(ctx, y++, x+4, "Super+Q          Close window", VUI_GRAY);
    vui_label(ctx, y++, x+4, "Super+Space      Toggle float", VUI_GRAY);
    vui_label(ctx, y++, x+4, "Alt+Tab          Cycle focus", VUI_GRAY);
    vui_label(ctx, y++, x+4, "Super+1-9        Switch workspace", VUI_GRAY);
    vui_label(ctx, y++, x+4, "Ctrl+Alt+BkSp    Quit VGP", VUI_GRAY);
}

/* ============================================================
 * Main render
 * ============================================================ */

static void render(vui_ctx_t *ctx)
{
    vui_clear(ctx, VUI_BG);
    vui_fill(ctx, 0, 0, 1, ctx->cols, VUI_SURFACE);
    vui_text_bold(ctx, 0, 2, " VGP Settings ", VUI_ACCENT, VUI_SURFACE);

    int tc = 2;
    for (int i = 0; i < TAB_COUNT; i++) {
        bool a = ((int)state.current_tab == i);
        bool h = (ctx->mouse_row == 2 && ctx->mouse_col >= tc && ctx->mouse_col < tc + (int)strlen(tab_names[i]) + 2);
        char b[32]; snprintf(b, sizeof(b), " %s ", tab_names[i]);
        vui_text(ctx, 2, tc, b, a ? VUI_ACCENT : (h ? VUI_WHITE : VUI_GRAY), a ? VUI_SURFACE : VUI_BG);
        if (h && ctx->mouse_clicked) { state.current_tab = i; state.edit_field = -1; }
        tc += (int)strlen(tab_names[i]) + 3;
    }
    vui_hline(ctx, 3, 0, ctx->cols, VUI_BORDER, VUI_BG);

    int cy = 5, cx = 2, cw = ctx->cols - 4;
    switch (state.current_tab) {
    case TAB_GENERAL:       render_general(ctx, cy, cx, cw); break;
    case TAB_PANEL:         render_panel(ctx, cy, cx, cw); break;
    case TAB_THEME:         render_themes(ctx, cy, cx, cw); break;
    case TAB_KEYBINDS:      render_keybinds(ctx, cy, cx, cw); break;
    case TAB_BACKGROUND:    render_background(ctx, cy, cx, cw); break;
    case TAB_MONITORS:      render_monitors(ctx, cy, cx, cw); break;
    case TAB_LOCKSCREEN:    render_lockscreen(ctx, cy, cx, cw); break;
    case TAB_ACCESSIBILITY: render_accessibility(ctx, cy, cx, cw); break;
    case TAB_ABOUT:         render_about(ctx, cy, cx, cw); break;
    default: break;
    }

    /* Status bar */
    vui_fill(ctx, ctx->rows - 1, 0, 1, ctx->cols, VUI_SURFACE);
    if (state.status_timer > 0) {
        vui_text(ctx, ctx->rows - 1, 2, state.status, VUI_GREEN, VUI_SURFACE);
        state.status_timer--;
    } else {
        vui_text(ctx, ctx->rows - 1, 2, "Click to interact | Esc to close", VUI_GRAY, VUI_SURFACE);
    }

    if (ctx->key_pressed && ctx->last_keysym == 0xFF1B && state.edit_field < 0)
        ctx->running = false;
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    FILE *lf = fopen("/tmp/vgp-settings.log", "w");
    if (lf) { setvbuf(lf, NULL, _IOLBF, 0); dup2(fileno(lf), STDERR_FILENO); fclose(lf); }

    load_config();
    scan_themes();

    vui_ctx_t ctx;
    if (vui_init(&ctx, "VGP Settings", 800, 600) < 0) return 1;
    vui_run(&ctx, render);
    vui_destroy(&ctx);
    return 0;
}
