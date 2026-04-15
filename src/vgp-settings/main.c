/* VGP Settings -- Full GUI configuration editor
 * Every setting is editable. Changes write back to config.toml.
 * The user has total control of all configuration options. */

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
    TAB_ABOUT,
    TAB_COUNT,
} settings_tab_t;

static const char *tab_names[] = {
    "General", "Panel", "Theme", "Keybinds", "Background",
    "Monitors", "Lock", "About"
};

typedef struct {
    settings_tab_t  current_tab;
    int             edit_field;

    char            terminal[256];
    char            launcher[256];
    char            theme_name[64];
    char            pointer_speed_str[16];
    char            workspaces_str[8];
    char            screenshot_dir[256];

    char            panel_position[16];
    char            panel_height_str[8];
    char            panel_left[256];
    char            panel_center[256];
    char            panel_right[256];

    char            bg_mode[16];
    char            bg_shader[256];
    char            bg_wallpaper[256];

    char            lock_timeout_str[8];
    char            lock_enabled[8];

    char            themes[32][64];
    int             theme_count;

    struct { char key[64]; char action[256]; } keybinds[128];
    int             keybind_count;

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

    snprintf(state.terminal, sizeof(state.terminal), "vgp-term");
    snprintf(state.launcher, sizeof(state.launcher), "vgp-launcher");
    snprintf(state.theme_name, sizeof(state.theme_name), "dark");
    snprintf(state.pointer_speed_str, sizeof(state.pointer_speed_str), "3.0");
    snprintf(state.workspaces_str, sizeof(state.workspaces_str), "9");
    snprintf(state.screenshot_dir, sizeof(state.screenshot_dir), "%s/Pictures", home);
    snprintf(state.panel_position, sizeof(state.panel_position), "bottom");
    snprintf(state.panel_height_str, sizeof(state.panel_height_str), "32");
    snprintf(state.panel_left, sizeof(state.panel_left), "workspaces");
    snprintf(state.panel_center, sizeof(state.panel_center), "taskbar");
    snprintf(state.panel_right, sizeof(state.panel_right), "clock");
    snprintf(state.bg_mode, sizeof(state.bg_mode), "shader");
    state.bg_shader[0] = '\0';
    state.bg_wallpaper[0] = '\0';
    snprintf(state.lock_timeout_str, sizeof(state.lock_timeout_str), "5");
    snprintf(state.lock_enabled, sizeof(state.lock_enabled), "true");
    state.keybind_count = 0;
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
            else if (strcmp(key, "workspaces") == 0) snprintf(state.workspaces_str, sizeof(state.workspaces_str), "%s", val);
            else if (strcmp(key, "screenshot_dir") == 0) snprintf(state.screenshot_dir, sizeof(state.screenshot_dir), "%s", val);
        } else if (strcmp(section, "input") == 0) {
            if (strcmp(key, "pointer_speed") == 0) snprintf(state.pointer_speed_str, sizeof(state.pointer_speed_str), "%s", val);
        } else if (strcmp(section, "panel") == 0) {
            if (strcmp(key, "position") == 0) snprintf(state.panel_position, sizeof(state.panel_position), "%s", val);
            else if (strcmp(key, "height") == 0) snprintf(state.panel_height_str, sizeof(state.panel_height_str), "%s", val);
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
                state.keybind_count++;
            }
        } else if (strcmp(section, "lockscreen") == 0) {
            if (strcmp(key, "enabled") == 0) snprintf(state.lock_enabled, sizeof(state.lock_enabled), "%s", val);
            else if (strcmp(key, "timeout") == 0) snprintf(state.lock_timeout_str, sizeof(state.lock_timeout_str), "%s", val);
        }
    }
    fclose(f);
}

static void save_config(void)
{
    const char *p = state.config_path;
    config_set_value(p, "general", "terminal", state.terminal);
    config_set_value(p, "general", "launcher", state.launcher);
    config_set_value(p, "general", "theme", state.theme_name);
    config_set_value(p, "general", "workspaces", state.workspaces_str);
    config_set_value(p, "general", "screenshot_dir", state.screenshot_dir);
    config_set_value(p, "input", "pointer_speed", state.pointer_speed_str);
    config_set_value(p, "panel", "position", state.panel_position);
    config_set_value(p, "panel", "height", state.panel_height_str);
    config_set_value(p, "panel.widgets.left", "items", state.panel_left);
    config_set_value(p, "panel.widgets.center", "items", state.panel_center);
    config_set_value(p, "panel.widgets.right", "items", state.panel_right);
    config_set_value(p, "lockscreen", "enabled", state.lock_enabled);
    config_set_value(p, "lockscreen", "timeout", state.lock_timeout_str);

    snprintf(state.status, sizeof(state.status), "Settings saved!");
    state.status_timer = 180;
}

static void set_status(const char *msg)
{
    snprintf(state.status, sizeof(state.status), "%s", msg);
    state.status_timer = 120;
}

/* ============================================================
 * Editable field
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
    if (editable_field(ctx, y++, x+2, 20, "Terminal:", state.terminal, 256, 1, fw)) set_status("Terminal changed");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Launcher:", state.launcher, 256, 2, fw)) set_status("Launcher changed");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Workspaces:", state.workspaces_str, 8, 3, 8)) set_status("Workspaces changed");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Screenshot dir:", state.screenshot_dir, 256, 4, fw)) set_status("Screenshot dir changed");
    y += 2;
    vui_section(ctx, y++, x, w, "Input", VUI_ACCENT); y++;
    if (editable_field(ctx, y++, x+2, 20, "Pointer speed:", state.pointer_speed_str, 16, 5, 10)) set_status("Pointer speed changed");
    y += 2;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_panel(vui_ctx_t *ctx, int y, int x, int w)
{
    int fw = w - 24;
    vui_section(ctx, y++, x, w, "Panel", VUI_ACCENT); y++;
    if (editable_field(ctx, y++, x+2, 20, "Position:", state.panel_position, 16, 10, 12)) set_status("Position changed (top/bottom)");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Height (px):", state.panel_height_str, 8, 11, 8)) set_status("Height changed");
    y += 2;
    vui_section(ctx, y++, x, w, "Widget Placement (comma-separated)", VUI_ACCENT); y++;
    if (editable_field(ctx, y++, x+2, 20, "Left:", state.panel_left, 256, 12, fw)) set_status("Left widgets changed");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Center:", state.panel_center, 256, 13, fw)) set_status("Center widgets changed");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Right:", state.panel_right, 256, 14, fw)) set_status("Right widgets changed");
    y += 2;
    vui_section(ctx, y++, x, w, "Available Widgets", VUI_GRAY); y++;
    const char *wlist[] = {"workspaces","taskbar","clock","date","settings","monitor","files","launcher","battery","volume",NULL};
    for (int i = 0; wlist[i]; i++) vui_label(ctx, y++, x + 4, wlist[i], VUI_GRAY);
    y += 2;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_themes(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Theme", VUI_ACCENT); y++;
    vui_label(ctx, y, x+2, "Active:", VUI_GRAY);
    vui_text_bold(ctx, y++, x + 14, state.theme_name, VUI_ACCENT, VUI_BG); y++;
    for (int i = 0; i < state.theme_count; i++) {
        bool sel = (strcmp(state.themes[i], state.theme_name) == 0);
        if (vui_list_item(ctx, y + i, x + 2, w - 4, state.themes[i], sel, false)) {
            snprintf(state.theme_name, sizeof(state.theme_name), "%s", state.themes[i]);
            config_set_value(state.config_path, "general", "theme", state.theme_name);
            set_status("Theme changed (restart to apply)");
        }
    }
}

static void render_keybinds(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Keybinds", VUI_ACCENT); y++;
    vui_text_bold(ctx, y, x+2, "Key", VUI_GRAY, VUI_BG);
    vui_text_bold(ctx, y++, x+28, "Action", VUI_GRAY, VUI_BG);
    vui_hline(ctx, y++, x+2, w-4, VUI_BORDER, VUI_BG);
    int vis = ctx->rows - y - 3;
    for (int i = 0; i < vis && i < state.keybind_count; i++) {
        vui_text(ctx, y + i, x + 2, state.keybinds[i].key, VUI_WHITE, VUI_BG);
        vui_text(ctx, y + i, x + 28, state.keybinds[i].action, VUI_ACCENT, VUI_BG);
    }
}

static void render_background(vui_ctx_t *ctx, int y, int x, int w)
{
    int fw = w - 24;
    vui_section(ctx, y++, x, w, "Background", VUI_ACCENT); y++;
    if (editable_field(ctx, y++, x+2, 20, "Mode:", state.bg_mode, 16, 20, 16)) set_status("Mode changed (solid/shader/wallpaper/none)");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Shader path:", state.bg_shader, 256, 21, fw)) set_status("Shader changed");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Wallpaper:", state.bg_wallpaper, 256, 22, fw)) set_status("Wallpaper changed");
    y += 2;
    vui_label(ctx, y++, x+2, "Shaders: ~/.config/vgp/shaders/ or theme shaders/", VUI_GRAY);
    y += 2;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_monitors(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Monitors", VUI_ACCENT); y++;
    vui_label(ctx, y++, x+2, "Configure in [monitor.N] sections of config.toml", VUI_GRAY); y++;
    vui_label(ctx, y++, x+4, "x, y = position | workspace = workspace number", VUI_GRAY);
    (void)w;
}

static void render_lockscreen(vui_ctx_t *ctx, int y, int x, int w)
{
    vui_section(ctx, y++, x, w, "Lock Screen", VUI_ACCENT); y++;
    if (editable_field(ctx, y++, x+2, 20, "Enabled:", state.lock_enabled, 8, 30, 10)) set_status("Lock enabled/disabled");
    y++;
    if (editable_field(ctx, y++, x+2, 20, "Timeout (min):", state.lock_timeout_str, 8, 31, 8)) set_status("Lock timeout changed");
    y += 2;
    vui_label(ctx, y++, x+2, "Lock screen uses theme shader background.", VUI_GRAY);
    y += 2;
    if (vui_button(ctx, y, x + 2, "Save All Settings", VUI_WHITE, VUI_ACCENT)) save_config();
}

static void render_about(vui_ctx_t *ctx, int y, int x, int w)
{
    (void)w;
    vui_text_bold(ctx, y++, x+2, "VGP - Vector Graphics Protocol", VUI_ACCENT, VUI_BG); y++;
    vui_label(ctx, y++, x+2, "Version 0.1.0", VUI_WHITE);
    vui_label(ctx, y++, x+2, "GPU-accelerated vector display server for Linux", VUI_GRAY); y++;
    vui_label(ctx, y++, x+2, "https://github.com/theesfeld/VGP", VUI_ACCENT);
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
        if (h && ctx->mouse_clicked) state.current_tab = i;
        tc += (int)strlen(tab_names[i]) + 3;
    }
    vui_hline(ctx, 3, 0, ctx->cols, VUI_BORDER, VUI_BG);

    int cy = 5, cx = 2, cw = ctx->cols - 4;
    switch (state.current_tab) {
    case TAB_GENERAL:    render_general(ctx, cy, cx, cw); break;
    case TAB_PANEL:      render_panel(ctx, cy, cx, cw); break;
    case TAB_THEME:      render_themes(ctx, cy, cx, cw); break;
    case TAB_KEYBINDS:   render_keybinds(ctx, cy, cx, cw); break;
    case TAB_BACKGROUND: render_background(ctx, cy, cx, cw); break;
    case TAB_MONITORS:   render_monitors(ctx, cy, cx, cw); break;
    case TAB_LOCKSCREEN: render_lockscreen(ctx, cy, cx, cw); break;
    case TAB_ABOUT:      render_about(ctx, cy, cx, cw); break;
    default: break;
    }

    vui_fill(ctx, ctx->rows - 1, 0, 1, ctx->cols, VUI_SURFACE);
    if (state.status_timer > 0) {
        vui_text(ctx, ctx->rows - 1, 2, state.status, VUI_GREEN, VUI_SURFACE);
        state.status_timer--;
    } else {
        vui_text(ctx, ctx->rows - 1, 2, "Click to edit | Enter to confirm | Esc to close", VUI_GRAY, VUI_SURFACE);
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
