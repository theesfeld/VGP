/* VGP Settings -- GUI configuration editor
 * Allows changing theme, keybinds, monitor layout, shaders, and all settings.
 * Uses the VGP cell grid UI framework. */

#include "vgp-ui.h"

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
    TAB_MONITORS,
    TAB_SHADERS,
    TAB_ABOUT,
    TAB_COUNT,
} settings_tab_t;

static const char *tab_names[] = {
    "General", "Panel", "Theme", "Keybinds", "Monitors", "Shaders", "About"
};

typedef struct {
    settings_tab_t  current_tab;
    int             tab_scroll[TAB_COUNT];
    int             tab_selected[TAB_COUNT];

    /* Theme list */
    char            themes[32][64];
    int             theme_count;
    int             theme_selected;

    /* Config values (loaded from file) */
    char            terminal[256];
    char            launcher[256];
    char            theme_name[64];
    float           pointer_speed;
    int             workspaces;

    /* Keybind list */
    struct { char key[64]; char action[256]; } keybinds[128];
    int             keybind_count;

    /* Panel config */
    char            panel_position[16];
    int             panel_height;
    char            panel_left[256];
    char            panel_center[256];
    char            panel_right[256];

    /* Status message */
    char            status[128];
    int             status_timer;
} settings_state_t;

static settings_state_t state;

/* ============================================================
 * Config loading
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
        snprintf(state.themes[state.theme_count], 64, "%s", entry->d_name);
        state.theme_count++;
    }
    closedir(dir);
}

static void load_config(void)
{
    const char *home = getenv("HOME");
    if (!home) return;

    char path[512];
    snprintf(path, sizeof(path), "%s/.config/vgp/config.toml", home);
    FILE *f = fopen(path, "r");
    if (!f) return;

    /* Defaults */
    snprintf(state.terminal, sizeof(state.terminal), "vgp-term");
    snprintf(state.launcher, sizeof(state.launcher), "vgp-launcher");
    snprintf(state.theme_name, sizeof(state.theme_name), "dark");
    state.pointer_speed = 3.0f;
    state.workspaces = 9;
    state.keybind_count = 0;
    snprintf(state.panel_position, sizeof(state.panel_position), "bottom");
    state.panel_height = 32;
    snprintf(state.panel_left, sizeof(state.panel_left), "workspaces");
    snprintf(state.panel_center, sizeof(state.panel_center), "taskbar");
    snprintf(state.panel_right, sizeof(state.panel_right), "clock");

    char line[512], section[64] = "";
    while (fgets(line, sizeof(line), f)) {
        /* Trim */
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
        } else if (strcmp(section, "input") == 0) {
            if (strcmp(key, "pointer_speed") == 0) state.pointer_speed = strtof(val, NULL);
        } else if (strcmp(section, "panel") == 0) {
            if (strcmp(key, "position") == 0) snprintf(state.panel_position, sizeof(state.panel_position), "%s", val);
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
                state.keybind_count++;
            }
        }
    }
    fclose(f);
}

static void set_status(const char *msg)
{
    snprintf(state.status, sizeof(state.status), "%s", msg);
    state.status_timer = 120; /* frames */
}

/* ============================================================
 * Tab renderers
 * ============================================================ */

static void render_general(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    vui_section(ctx, row++, start_col, width, "General Settings", VUI_ACCENT);
    row++;
    vui_label(ctx, row, start_col + 2, "Terminal command:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 22, state.terminal, VUI_WHITE, VUI_BG);
    row++;
    vui_label(ctx, row, start_col + 2, "Launcher command:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 22, state.launcher, VUI_WHITE, VUI_BG);
    row++;
    vui_label(ctx, row, start_col + 2, "Workspaces:", VUI_GRAY);
    char ws_str[8]; snprintf(ws_str, sizeof(ws_str), "%d", state.workspaces);
    vui_text(ctx, row++, start_col + 22, ws_str, VUI_WHITE, VUI_BG);
    row++;
    vui_label(ctx, row, start_col + 2, "Pointer speed:", VUI_GRAY);
    char ps_str[16]; snprintf(ps_str, sizeof(ps_str), "%.1f", state.pointer_speed);
    vui_text(ctx, row++, start_col + 22, ps_str, VUI_WHITE, VUI_BG);
    row += 2;

    vui_section(ctx, row++, start_col, width, "Background", VUI_ACCENT);
    row++;
    vui_label(ctx, row, start_col + 2, "Mode:", VUI_GRAY);
    const char *bg_modes[] = {"solid", "shader", "wallpaper", "none"};
    vui_text(ctx, row++, start_col + 22, bg_modes[0], VUI_WHITE, VUI_BG);
    row++;
    vui_label(ctx, row++, start_col + 2, "Options: solid, shader, wallpaper, none", VUI_GRAY);
    vui_label(ctx, row++, start_col + 2, "Shader and wallpaper paths set in theme.toml", VUI_GRAY);
    (void)bg_modes;
}

static void render_panel(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    vui_section(ctx, row++, start_col, width, "Panel Settings", VUI_ACCENT);
    row++;

    vui_label(ctx, row, start_col + 2, "Position:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 22, state.panel_position, VUI_WHITE, VUI_BG);

    vui_label(ctx, row, start_col + 2, "Height:", VUI_GRAY);
    char h_str[8]; snprintf(h_str, sizeof(h_str), "%d px", state.panel_height);
    vui_text(ctx, row++, start_col + 22, h_str, VUI_WHITE, VUI_BG);
    row++;

    vui_section(ctx, row++, start_col, width, "Widget Placement", VUI_ACCENT);
    row++;
    vui_label(ctx, row, start_col + 2, "Left widgets:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 22, state.panel_left, VUI_WHITE, VUI_BG);
    row++;
    vui_label(ctx, row, start_col + 2, "Center widgets:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 22, state.panel_center, VUI_WHITE, VUI_BG);
    row++;
    vui_label(ctx, row, start_col + 2, "Right widgets:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 22, state.panel_right, VUI_WHITE, VUI_BG);
    row += 2;

    vui_section(ctx, row++, start_col, width, "Available Widgets", VUI_GRAY);
    row++;
    const struct { const char *name; const char *desc; } widgets[] = {
        {"workspaces", "Workspace indicators (click to switch)"},
        {"taskbar",    "Window list (click to focus)"},
        {"clock",      "Current time"},
        {"date",       "Current date"},
        {"settings",   "Settings button (opens vgp-settings)"},
        {"monitor",    "CPU/RAM mini-indicator"},
        {"files",      "File manager button"},
        {"launcher",   "Application launcher button"},
        {"battery",    "Battery status (laptops)"},
        {"volume",     "Volume control"},
        {NULL, NULL},
    };
    for (int i = 0; widgets[i].name; i++) {
        char buf[128];
        snprintf(buf, sizeof(buf), "  %-14s %s", widgets[i].name, widgets[i].desc);
        vui_label(ctx, row++, start_col + 2, buf, VUI_GRAY);
    }
}

static void render_themes(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    vui_section(ctx, row++, start_col, width, "Theme Selection", VUI_ACCENT);
    row++;
    vui_label(ctx, row++, start_col + 2, "Active theme:", VUI_GRAY);
    vui_text_bold(ctx, row++, start_col + 4, state.theme_name, VUI_ACCENT, VUI_BG);
    row++;
    vui_section(ctx, row++, start_col, width, "Installed Themes", VUI_GRAY);
    row++;

    for (int i = 0; i < state.theme_count; i++) {
        bool selected = (strcmp(state.themes[i], state.theme_name) == 0);
        bool clicked = vui_list_item(ctx, row + i, start_col + 2, width - 4,
                                      state.themes[i], selected, false);
        if (clicked) {
            snprintf(state.theme_name, sizeof(state.theme_name), "%s", state.themes[i]);
            set_status("Theme changed (restart VGP to apply)");
        }
    }
}

static void render_keybinds(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    vui_section(ctx, row++, start_col, width, "Keybinds", VUI_ACCENT);
    row++;

    /* Header */
    vui_text_bold(ctx, row, start_col + 2, "Key", VUI_GRAY, VUI_BG);
    vui_text_bold(ctx, row++, start_col + 26, "Action", VUI_GRAY, VUI_BG);
    vui_hline(ctx, row++, start_col + 2, width - 4, VUI_BORDER, VUI_BG);

    int visible = ctx->rows - row - 3;
    int offset = state.tab_scroll[TAB_KEYBINDS];
    if (offset > state.keybind_count - visible) offset = state.keybind_count - visible;
    if (offset < 0) offset = 0;

    for (int i = 0; i < visible && offset + i < state.keybind_count; i++) {
        int idx = offset + i;
        vui_text(ctx, row + i, start_col + 2, state.keybinds[idx].key, VUI_WHITE, VUI_BG);
        vui_text(ctx, row + i, start_col + 26, state.keybinds[idx].action, VUI_ACCENT, VUI_BG);
    }

    vui_scrollbar(ctx, start_row + 4, start_col + width - 1,
                   visible, visible, state.keybind_count, offset);
}

static void render_monitors(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    vui_section(ctx, row++, start_col, width, "Monitor Configuration", VUI_ACCENT);
    row++;
    vui_label(ctx, row++, start_col + 2,
              "Monitor layout is configured in config.toml [monitor.N] sections.", VUI_GRAY);
    row++;
    vui_label(ctx, row++, start_col + 2,
              "Drag-to-arrange GUI coming in Phase 7.", VUI_GRAY);
}

static void render_shaders(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    vui_section(ctx, row++, start_col, width, "Shader Effects", VUI_ACCENT);
    row++;
    vui_label(ctx, row++, start_col + 2,
              "Shaders are .frag files in ~/.config/vgp/shaders/", VUI_GRAY);
    vui_label(ctx, row++, start_col + 2,
              "or in the active theme's shaders/ directory.", VUI_GRAY);
    row++;
    vui_label(ctx, row++, start_col + 2,
              "Background shader:", VUI_GRAY);
    vui_text(ctx, row++, start_col + 4, "~/.config/vgp/shaders/background.frag", VUI_ACCENT, VUI_BG);
    row++;
    vui_label(ctx, row++, start_col + 2,
              "Shader browser and live editor coming soon.", VUI_GRAY);
}

static void render_about(vui_ctx_t *ctx, int start_row, int start_col, int width)
{
    int row = start_row;
    (void)width;
    vui_text_bold(ctx, row++, start_col + 2, "VGP - Vector Graphics Protocol", VUI_ACCENT, VUI_BG);
    row++;
    vui_label(ctx, row++, start_col + 2, "Version 0.1.0", VUI_WHITE);
    row++;
    vui_label(ctx, row++, start_col + 2, "A GPU-accelerated vector display server", VUI_GRAY);
    vui_label(ctx, row++, start_col + 2, "for Linux. All rendering is vector-based.", VUI_GRAY);
    vui_label(ctx, row++, start_col + 2, "No X11. No Wayland. Pure VGP.", VUI_GRAY);
    row++;
    vui_label(ctx, row++, start_col + 2, "GPU Backend: NanoVG + OpenGL ES 3.0", VUI_GRAY);
    vui_label(ctx, row++, start_col + 2, "IPC: Cell grid protocol (vector text)", VUI_GRAY);
    vui_label(ctx, row++, start_col + 2, "Session: libseat (runs without root)", VUI_GRAY);
}

/* ============================================================
 * Main render function
 * ============================================================ */

static void render(vui_ctx_t *ctx)
{
    vui_clear(ctx, VUI_BG);

    /* Title bar */
    vui_fill(ctx, 0, 0, 1, ctx->cols, VUI_SURFACE);
    vui_text_bold(ctx, 0, 2, " VGP Settings ", VUI_ACCENT, VUI_SURFACE);

    /* Tab bar */
    int tab_col = 2;
    for (int i = 0; i < TAB_COUNT; i++) {
        bool is_active = (state.current_tab == i);
        bool hover = (ctx->mouse_row == 2 &&
                       ctx->mouse_col >= tab_col &&
                       ctx->mouse_col < tab_col + (int)strlen(tab_names[i]) + 2);

        vui_color_t fg = is_active ? VUI_ACCENT : (hover ? VUI_WHITE : VUI_GRAY);
        vui_color_t bg = is_active ? VUI_SURFACE : VUI_BG;

        char buf[32];
        snprintf(buf, sizeof(buf), " %s ", tab_names[i]);
        vui_text(ctx, 2, tab_col, buf, fg, bg);

        if (hover && ctx->mouse_clicked)
            state.current_tab = i;

        tab_col += (int)strlen(tab_names[i]) + 3;
    }

    /* Tab underline */
    vui_hline(ctx, 3, 0, ctx->cols, VUI_BORDER, VUI_BG);

    /* Content area */
    int content_start = 5;
    int content_width = ctx->cols - 4;

    switch (state.current_tab) {
    case TAB_GENERAL:  render_general(ctx, content_start, 2, content_width); break;
    case TAB_PANEL:    render_panel(ctx, content_start, 2, content_width); break;
    case TAB_THEME:    render_themes(ctx, content_start, 2, content_width); break;
    case TAB_KEYBINDS: render_keybinds(ctx, content_start, 2, content_width); break;
    case TAB_MONITORS: render_monitors(ctx, content_start, 2, content_width); break;
    case TAB_SHADERS:  render_shaders(ctx, content_start, 2, content_width); break;
    case TAB_ABOUT:    render_about(ctx, content_start, 2, content_width); break;
    default: break;
    }

    /* Status bar at bottom */
    vui_fill(ctx, ctx->rows - 1, 0, 1, ctx->cols, VUI_SURFACE);
    if (state.status_timer > 0) {
        vui_text(ctx, ctx->rows - 1, 2, state.status, VUI_GREEN, VUI_SURFACE);
        state.status_timer--;
    } else {
        vui_text(ctx, ctx->rows - 1, 2, "Use Tab/Arrow keys or mouse to navigate",
                  VUI_GRAY, VUI_SURFACE);
    }

    /* Handle Escape to quit */
    if (ctx->key_pressed && ctx->last_keysym == 0xFF1B)
        ctx->running = false;
}

/* ============================================================
 * Entry point
 * ============================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    FILE *logfile = fopen("/tmp/vgp-settings.log", "w");
    if (logfile) {
        setvbuf(logfile, NULL, _IOLBF, 0);
        dup2(fileno(logfile), STDERR_FILENO);
        fclose(logfile);
    }

    load_config();
    scan_themes();

    vui_ctx_t ctx;
    if (vui_init(&ctx, "VGP Settings", 720, 500) < 0)
        return 1;

    vui_run(&ctx, render);
    vui_destroy(&ctx);
    return 0;
}
