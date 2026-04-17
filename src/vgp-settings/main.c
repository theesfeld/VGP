/* VGP Settings -- Complete configuration GUI
 * Sidebar navigation, every config option present, proper widgets.
 * All vector-rendered. */

#include "vgp-gfx.h"
#include "vgp-hud.h"
#include "config-writer.h"
#include "vgp/xdg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>

/* ============================================================
 * Layout constants
 * ============================================================ */

#define SIDEBAR_W 20
#define SIDEBAR_ICON_W 3

/* ============================================================
 * Application state
 * ============================================================ */

typedef enum {
    PAGE_GENERAL,
    PAGE_INPUT,
    PAGE_WINDOWS,
    PAGE_PANEL,
    PAGE_THEME,
    PAGE_BACKGROUND,
    PAGE_KEYBINDS,
    PAGE_MONITORS,
    PAGE_AUTOSTART,
    PAGE_RULES,
    PAGE_LOCKSCREEN,
    PAGE_ACCESSIBILITY,
    PAGE_ABOUT,
    PAGE_COUNT,
} settings_page_t;

static const char *page_labels[] = {
    "  General",
    "  Input",
    "  Windows",
    "  Panel",
    "  Theme",
    "  Background",
    "  Keybinds",
    "  Monitors",
    "  Autostart",
    "  Rules",
    "  Lock Screen",
    "  Accessibility",
    "  About",
};

/* Sidebar uses text labels only (stroke font is ASCII) */

typedef struct {
    settings_page_t current_page;
    int             edit_field;
    int             scroll_y;  /* vertical scroll for content area */

    /* General */
    char            terminal[256];
    char            launcher[256];
    char            screenshot_dir[256];
    char            url_handler[256];
    char            font_path[256];
    float           font_size;
    int             workspaces;
    bool            focus_follows_mouse;

    /* Input */
    float           pointer_speed;
    bool            natural_scrolling;
    bool            tap_to_click;
    int             repeat_delay;
    int             repeat_rate;

    /* Windows */
    int             wm_mode;
    int             tile_algo;
    float           tile_master_ratio;
    int             tile_gap_inner;
    int             tile_gap_outer;
    bool            tile_smart_gaps;

    /* Panel */
    int             panel_position;
    int             panel_height;
    char            panel_left[256];
    char            panel_center[256];
    char            panel_right[256];

    /* Background */
    int             bg_mode;
    char            bg_shader[256];
    char            bg_wallpaper[256];

    /* Lock screen */
    bool            lock_enabled;
    int             lock_timeout;

    /* Accessibility */
    bool            a11y_high_contrast;
    bool            a11y_focus_indicator;
    float           a11y_text_size;
    bool            a11y_reduce_anims;
    bool            a11y_large_cursor;

    /* Theme */
    char            theme_name[64];
    char            themes[32][64];
    int             theme_count;

    /* Keybinds */
    struct { char key[64]; char action[256]; bool capturing; } keybinds[128];
    int             keybind_count;

    /* Autostart */
    char            autostart[16][256];
    int             autostart_count;

    /* Window rules */
    struct { char title[128]; bool floating; int workspace; } rules[32];
    int             rule_count;

    /* Monitors */
    struct { bool configured; int x, y, workspace; } monitors[8];
    int             monitor_count;

    /* Dropdown states */
    bool            dd_wm_mode;
    bool            dd_tile_algo;
    bool            dd_panel_pos;
    bool            dd_bg_mode;

    bool            unsaved;
    char            status[128];
    int             status_timer;
    char            config_path[512];
} settings_state_t;

static settings_state_t S;

/* ============================================================
 * Config loading / saving
 * ============================================================ */

static void scan_themes_dir(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && S.theme_count < 32) {
        if (entry->d_name[0] == '.') continue;
        if (entry->d_type != DT_DIR) continue;
        /* de-dup */
        bool seen = false;
        for (int i = 0; i < S.theme_count; i++)
            if (strcmp(S.themes[i], entry->d_name) == 0) { seen = true; break; }
        if (!seen)
            snprintf(S.themes[S.theme_count++], 64, "%s", entry->d_name);
    }
    closedir(dir);
}

static void scan_themes(void)
{
    S.theme_count = 0;
    /* 1. $XDG_CONFIG_HOME/vgp/themes */
    char p[512];
    if (vgp_xdg_resolve(VGP_XDG_CONFIG, "vgp/themes", p, sizeof(p)))
        scan_themes_dir(p);
    /* 2. $XDG_DATA_HOME/vgp/themes */
    if (vgp_xdg_resolve(VGP_XDG_DATA, "vgp/themes", p, sizeof(p)))
        scan_themes_dir(p);
    /* 3. $XDG_DATA_DIRS/vgp/themes */
    const char *dirs = getenv("XDG_DATA_DIRS");
    if (!dirs || !*dirs) dirs = "/usr/local/share:/usr/share";
    const char *q = dirs;
    while (*q) {
        const char *end = strchr(q, ':');
        size_t len = end ? (size_t)(end - q) : strlen(q);
        if (len > 0 && len < sizeof(p) - 32) {
            char dir[512];
            memcpy(dir, q, len); dir[len] = '\0';
            snprintf(p, sizeof(p), "%s/vgp/themes", dir);
            scan_themes_dir(p);
        }
        if (!end) break;
        q = end + 1;
    }
}

static void load_config(void)
{
    /* Config path: $XDG_CONFIG_HOME/vgp/config.toml */
    if (!vgp_xdg_resolve(VGP_XDG_CONFIG, "vgp/config.toml",
                           S.config_path, sizeof(S.config_path)))
        return;

    const char *home = getenv("HOME");

    /* Defaults */
    snprintf(S.terminal, sizeof(S.terminal), "vgp-term");
    snprintf(S.launcher, sizeof(S.launcher), "vgp-launcher");
    snprintf(S.theme_name, sizeof(S.theme_name), "dark");
    const char *xdg_pics = getenv("XDG_PICTURES_DIR");
    if (xdg_pics && *xdg_pics)
        snprintf(S.screenshot_dir, sizeof(S.screenshot_dir), "%s", xdg_pics);
    else if (home)
        snprintf(S.screenshot_dir, sizeof(S.screenshot_dir), "%s/Pictures", home);
    snprintf(S.url_handler, sizeof(S.url_handler), "vgp-term -e w3m '%%s'");
    S.font_size = 14.0f;
    S.workspaces = 9;
    S.pointer_speed = 3.0f;
    S.repeat_delay = 300;
    S.repeat_rate = 30;
    S.panel_height = 32;
    snprintf(S.panel_left, sizeof(S.panel_left), "workspaces");
    snprintf(S.panel_center, sizeof(S.panel_center), "taskbar");
    snprintf(S.panel_right, sizeof(S.panel_right), "clock, date");
    S.tile_master_ratio = 0.55f;
    S.tile_gap_inner = 6;
    S.tile_gap_outer = 8;
    S.tile_smart_gaps = true;
    S.bg_mode = 1;
    S.lock_enabled = true;
    S.lock_timeout = 5;
    S.edit_field = -1;

    FILE *f = fopen(S.config_path, "r");
    if (!f) return;

    char line[512], section[64] = "";
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r' || line[len-1] == ' '))
            line[--len] = '\0';
        char *s = line; while (*s == ' ') s++;
        if (s[0] == '#' || s[0] == '\0') continue;
        if (s[0] == '[') { char *e = strchr(s,']'); if (e) { *e='\0'; snprintf(section,64,"%s",s+1); } continue; }
        char *eq = strchr(s, '='); if (!eq) continue;
        *eq = '\0'; char *key = s, *val = eq+1;
        while (*key == ' ') key++; len = strlen(key); while (len>0 && key[len-1]==' ') key[--len]='\0';
        while (*val == ' ') val++; len = strlen(val);
        if (len >= 2 && val[0]=='"' && val[len-1]=='"') { val[len-1]='\0'; val++; }

        if (strcmp(section,"general")==0) {
            if (strcmp(key,"terminal")==0) snprintf(S.terminal,256,"%s",val);
            else if (strcmp(key,"launcher")==0) snprintf(S.launcher,256,"%s",val);
            else if (strcmp(key,"theme")==0) snprintf(S.theme_name,64,"%s",val);
            else if (strcmp(key,"workspaces")==0) S.workspaces = atoi(val);
            else if (strcmp(key,"screenshot_dir")==0) snprintf(S.screenshot_dir,256,"%s",val);
            else if (strcmp(key,"url_handler")==0) snprintf(S.url_handler,256,"%s",val);
            else if (strcmp(key,"font_size")==0) S.font_size = (float)atof(val);
            else if (strcmp(key,"font_path")==0) snprintf(S.font_path,256,"%s",val);
            else if (strcmp(key,"focus_follows_mouse")==0) S.focus_follows_mouse = strcmp(val,"true")==0;
            else if (strcmp(key,"wm_mode")==0) {
                if (strcmp(val,"tiling")==0) S.wm_mode=1; else if (strcmp(val,"hybrid")==0) S.wm_mode=2; else S.wm_mode=0;
            }
            else if (strcmp(key,"tile_algorithm")==0) {
                if (strcmp(val,"equal")==0) S.tile_algo=1; else if (strcmp(val,"master_stack")==0) S.tile_algo=2;
                else if (strcmp(val,"spiral")==0) S.tile_algo=3; else S.tile_algo=0;
            }
            else if (strcmp(key,"tile_master_ratio")==0) S.tile_master_ratio=(float)atof(val);
            else if (strcmp(key,"tile_gap_inner")==0) S.tile_gap_inner=atoi(val);
            else if (strcmp(key,"tile_gap_outer")==0) S.tile_gap_outer=atoi(val);
            else if (strcmp(key,"tile_smart_gaps")==0) S.tile_smart_gaps=strcmp(val,"true")==0;
        } else if (strcmp(section,"input")==0) {
            if (strcmp(key,"pointer_speed")==0) S.pointer_speed=(float)atof(val);
            else if (strcmp(key,"natural_scrolling")==0) S.natural_scrolling=strcmp(val,"true")==0;
            else if (strcmp(key,"tap_to_click")==0) S.tap_to_click=strcmp(val,"true")==0;
            else if (strcmp(key,"repeat_delay")==0) S.repeat_delay=atoi(val);
            else if (strcmp(key,"repeat_rate")==0) S.repeat_rate=atoi(val);
        } else if (strcmp(section,"panel")==0) {
            if (strcmp(key,"position")==0) S.panel_position=strcmp(val,"top")==0?1:0;
            else if (strcmp(key,"height")==0) S.panel_height=atoi(val);
        } else if (strcmp(section,"panel.widgets.left")==0) {
            if (strcmp(key,"items")==0) snprintf(S.panel_left,256,"%s",val);
        } else if (strcmp(section,"panel.widgets.center")==0) {
            if (strcmp(key,"items")==0) snprintf(S.panel_center,256,"%s",val);
        } else if (strcmp(section,"panel.widgets.right")==0) {
            if (strcmp(key,"items")==0) snprintf(S.panel_right,256,"%s",val);
        } else if (strcmp(section,"keybinds")==0) {
            if (S.keybind_count<128) {
                snprintf(S.keybinds[S.keybind_count].key,64,"%s",key);
                snprintf(S.keybinds[S.keybind_count].action,256,"%s",val);
                S.keybind_count++;
            }
        } else if (strcmp(section,"lockscreen")==0) {
            if (strcmp(key,"enabled")==0) S.lock_enabled=strcmp(val,"true")==0;
            else if (strcmp(key,"timeout")==0) S.lock_timeout=atoi(val);
        } else if (strcmp(section,"accessibility")==0) {
            if (strcmp(key,"high_contrast")==0) S.a11y_high_contrast=strcmp(val,"true")==0;
            else if (strcmp(key,"focus_indicator")==0) S.a11y_focus_indicator=strcmp(val,"true")==0;
            else if (strcmp(key,"text_size")==0||strcmp(key,"font_scale")==0) S.a11y_text_size=(float)atof(val);
            else if (strcmp(key,"reduce_animations")==0) S.a11y_reduce_anims=strcmp(val,"true")==0;
            else if (strcmp(key,"large_cursor")==0) S.a11y_large_cursor=strcmp(val,"true")==0;
        } else if (strcmp(section,"session")==0) {
            if (strcmp(key,"autostart")==0 && S.autostart_count<16)
                snprintf(S.autostart[S.autostart_count++],256,"%s",val);
        } else if (strncmp(section,"rule.",5)==0) {
            int idx=atoi(section+5);
            if (idx>=0 && idx<32) {
                if (strcmp(key,"title")==0) snprintf(S.rules[idx].title,128,"%s",val);
                else if (strcmp(key,"floating")==0) S.rules[idx].floating=strcmp(val,"true")==0;
                else if (strcmp(key,"workspace")==0) S.rules[idx].workspace=atoi(val);
                if (idx>=S.rule_count) S.rule_count=idx+1;
            }
        } else if (strncmp(section,"monitor.",8)==0) {
            int idx=atoi(section+8);
            if (idx>=0 && idx<8) {
                S.monitors[idx].configured=true;
                if (strcmp(key,"x")==0) S.monitors[idx].x=atoi(val);
                else if (strcmp(key,"y")==0) S.monitors[idx].y=atoi(val);
                else if (strcmp(key,"workspace")==0) S.monitors[idx].workspace=atoi(val);
                if (idx>=S.monitor_count) S.monitor_count=idx+1;
            }
        }
    }
    fclose(f);
}

static const char *wm_modes[]={"floating","tiling","hybrid"};
static const char *tile_algos[]={"golden_ratio","equal","master_stack","spiral"};
static const char *panel_positions[]={"bottom","top"};
static const char *bg_modes[]={"solid","shader","wallpaper","none"};

static void save_config(void)
{
    const char *p=S.config_path; char b[32];
    config_set_value(p,"general","terminal",S.terminal);
    config_set_value(p,"general","launcher",S.launcher);
    config_set_value(p,"general","theme",S.theme_name);
    snprintf(b,32,"%d",S.workspaces); config_set_value(p,"general","workspaces",b);
    config_set_value(p,"general","screenshot_dir",S.screenshot_dir);
    config_set_value(p,"general","url_handler",S.url_handler);
    config_set_value(p,"general","focus_follows_mouse",S.focus_follows_mouse?"true":"false");
    snprintf(b,32,"%.1f",S.font_size); config_set_value(p,"general","font_size",b);
    config_set_value(p,"general","wm_mode",wm_modes[S.wm_mode]);
    config_set_value(p,"general","tile_algorithm",tile_algos[S.tile_algo]);
    snprintf(b,32,"%.2f",S.tile_master_ratio); config_set_value(p,"general","tile_master_ratio",b);
    snprintf(b,32,"%d",S.tile_gap_inner); config_set_value(p,"general","tile_gap_inner",b);
    snprintf(b,32,"%d",S.tile_gap_outer); config_set_value(p,"general","tile_gap_outer",b);
    config_set_value(p,"general","tile_smart_gaps",S.tile_smart_gaps?"true":"false");
    snprintf(b,32,"%.1f",S.pointer_speed); config_set_value(p,"input","pointer_speed",b);
    config_set_value(p,"input","natural_scrolling",S.natural_scrolling?"true":"false");
    config_set_value(p,"input","tap_to_click",S.tap_to_click?"true":"false");
    snprintf(b,32,"%d",S.repeat_delay); config_set_value(p,"input","repeat_delay",b);
    snprintf(b,32,"%d",S.repeat_rate); config_set_value(p,"input","repeat_rate",b);
    config_set_value(p,"panel","position",panel_positions[S.panel_position]);
    snprintf(b,32,"%d",S.panel_height); config_set_value(p,"panel","height",b);
    config_set_value(p,"panel.widgets.left","items",S.panel_left);
    config_set_value(p,"panel.widgets.center","items",S.panel_center);
    config_set_value(p,"panel.widgets.right","items",S.panel_right);
    config_set_value(p,"lockscreen","enabled",S.lock_enabled?"true":"false");
    snprintf(b,32,"%d",S.lock_timeout); config_set_value(p,"lockscreen","timeout",b);
    config_set_value(p,"accessibility","high_contrast",S.a11y_high_contrast?"true":"false");
    config_set_value(p,"accessibility","focus_indicator",S.a11y_focus_indicator?"true":"false");
    snprintf(b,32,"%.0f",S.a11y_text_size); config_set_value(p,"accessibility","text_size",b);
    config_set_value(p,"accessibility","reduce_animations",S.a11y_reduce_anims?"true":"false");
    config_set_value(p,"accessibility","large_cursor",S.a11y_large_cursor?"true":"false");

    /* Save monitor config */
    for (int i=0; i<S.monitor_count; i++) {
        char sec[32]; snprintf(sec,32,"monitor.%d",i);
        snprintf(b,32,"%d",S.monitors[i].x); config_set_value(p,sec,"x",b);
        snprintf(b,32,"%d",S.monitors[i].y); config_set_value(p,sec,"y",b);
        snprintf(b,32,"%d",S.monitors[i].workspace); config_set_value(p,sec,"workspace",b);
    }

    S.unsaved=false;
    snprintf(S.status,128,"Saved. Reload: SIGHUP or restart VGP.");
    S.status_timer=180;
}

static void mark_changed(const char *msg)
{
    S.unsaved=true;
    snprintf(S.status,128,"%s",msg);
    S.status_timer=90;
}

/* ============================================================
 * Rendering helpers (pixel coordinates)
 * ============================================================ */

#define LH (ctx->theme.font_size + 8)  /* line height */
#define FS (ctx->theme.font_size)
#define P  (ctx->theme.padding)

static void desc_text(vgfx_ctx_t *ctx, float *y, float x, const char *text)
{
    vgfx_text(ctx, text, x, *y + FS * 0.8f, FS - 2,
               vgfx_theme_color(ctx, VGP_THEME_FG_DISABLED));
    *y += LH * 0.7f;
}

static bool labeled_input(vgfx_ctx_t *ctx, float *y, float x, float w,
                            const char *label, char *buf, int buf_sz)
{
    vgfx_label(ctx, x, *y, label);
    float ix = x + w * 0.3f;
    bool r = vgfx_text_input(ctx, ix, *y, w * 0.7f, buf, buf_sz);
    *y += LH + 4;
    return r;
}

/* ============================================================
 * Page renderers
 * ============================================================ */

static void page_general(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Applications"); y += LH * 1.5f;
    if (labeled_input(ctx, &y, x, w, "Terminal", S.terminal, 256)) mark_changed("Terminal changed");
    desc_text(ctx, &y, x, "Command for Super+Return"); y += 4;
    if (labeled_input(ctx, &y, x, w, "Launcher", S.launcher, 256)) mark_changed("Launcher changed");
    desc_text(ctx, &y, x, "Command for Super+D"); y += 4;
    if (labeled_input(ctx, &y, x, w, "URL handler", S.url_handler, 256)) mark_changed("URL handler changed");
    desc_text(ctx, &y, x, "Opens URLs from terminal. Use %s for the URL"); y += 4;
    if (labeled_input(ctx, &y, x, w, "Screenshots", S.screenshot_dir, 256)) mark_changed("Screenshot dir changed");
    y += 8;

    vgfx_section(ctx, x, y, w, "Desktop"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Workspaces");
    float ws_f = (float)S.workspaces;
    if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &ws_f, 1, 9, "%.0f"))
        { S.workspaces = (int)ws_f; mark_changed("Workspaces changed"); }
    y += LH + 4;
    if (vgfx_checkbox(ctx, x, y, "Focus follows mouse", &S.focus_follows_mouse))
        mark_changed("Focus mode changed");
    desc_text(ctx, &(float){y += LH + 4}, x, "Window under cursor gets focus automatically");
}

static void page_input(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Pointer"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Speed");
    if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &S.pointer_speed, 0.5f, 10.0f, "%.1f"))
        mark_changed("Pointer speed changed");
    y += LH + 4;
    if (vgfx_checkbox(ctx, x, y, "Natural scrolling", &S.natural_scrolling))
        mark_changed("Natural scrolling toggled");
    y += LH + 4;
    if (vgfx_checkbox(ctx, x, y, "Tap to click", &S.tap_to_click))
        mark_changed("Tap to click toggled");
    y += LH + 12;

    vgfx_section(ctx, x, y, w, "Keyboard"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Repeat delay");
    float rd = (float)S.repeat_delay;
    if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &rd, 100, 1000, "%.0f ms"))
        { S.repeat_delay = (int)rd; mark_changed("Repeat delay changed"); }
    y += LH + 4;
    vgfx_label(ctx, x, y, "Repeat rate");
    float rr = (float)S.repeat_rate;
    if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &rr, 10, 100, "%.0f ms"))
        { S.repeat_rate = (int)rr; mark_changed("Repeat rate changed"); }
}

static void page_windows(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Window Management"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Mode");
    if (vgfx_dropdown(ctx, x + w*0.3f, y, w*0.35f, wm_modes, 3, &S.wm_mode, &S.dd_wm_mode))
        mark_changed("WM mode changed");
    y += S.dd_wm_mode ? LH * 5 : LH + 8;
    desc_text(ctx, &y, x, "floating = free, tiling = auto-tile, hybrid = mix"); y += 4;

    if (S.wm_mode >= 1) {
        vgfx_section(ctx, x, y, w, "Tiling"); y += LH * 1.5f;
        vgfx_label(ctx, x, y, "Algorithm");
        if (vgfx_dropdown(ctx, x + w*0.3f, y, w*0.35f, tile_algos, 4, &S.tile_algo, &S.dd_tile_algo))
            mark_changed("Tile algorithm changed");
        y += S.dd_tile_algo ? LH * 6 : LH + 8;

        vgfx_label(ctx, x, y, "Master ratio");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &S.tile_master_ratio, 0.2f, 0.8f, "%.2f"))
            mark_changed("Master ratio changed");
        y += LH + 4;
        float gi = (float)S.tile_gap_inner;
        vgfx_label(ctx, x, y, "Inner gap");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &gi, 0, 24, "%.0f px"))
            { S.tile_gap_inner = (int)gi; mark_changed("Inner gap changed"); }
        y += LH + 2;
        float go = (float)S.tile_gap_outer;
        vgfx_label(ctx, x, y, "Outer gap");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &go, 0, 24, "%.0f px"))
            { S.tile_gap_outer = (int)go; mark_changed("Outer gap changed"); }
        y += LH + 4;
        if (vgfx_checkbox(ctx, x, y, "Smart gaps (remove when 1 window)", &S.tile_smart_gaps))
            mark_changed("Smart gaps toggled");
    }
}

static void page_panel(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Panel Layout"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Position");
    if (vgfx_dropdown(ctx, x + w*0.3f, y, w*0.25f, panel_positions, 2, &S.panel_position, &S.dd_panel_pos))
        mark_changed("Panel position changed");
    y += S.dd_panel_pos ? LH * 4 : LH + 8;

    float ph = (float)S.panel_height;
    vgfx_label(ctx, x, y, "Height");
    if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &ph, 20, 48, "%.0f px"))
        { S.panel_height = (int)ph; mark_changed("Panel height changed"); }
    y += LH + 12;

    vgfx_section(ctx, x, y, w, "Widgets (comma-separated)"); y += LH * 1.5f;
    if (labeled_input(ctx, &y, x, w, "Left", S.panel_left, 256)) mark_changed("Left widgets changed");
    if (labeled_input(ctx, &y, x, w, "Center", S.panel_center, 256)) mark_changed("Center widgets changed");
    if (labeled_input(ctx, &y, x, w, "Right", S.panel_right, 256)) mark_changed("Right widgets changed");
    y += 8;

    vgfx_section(ctx, x, y, w, "Available Widgets"); y += LH * 1.2f;
    const char *wl[][2] = {
        {"workspaces","Workspace indicators"},{"taskbar","Window list"},
        {"clock","Time HH:MM"},{"date","Date MM/DD"},{"cpu","CPU %"},
        {"memory","Memory %"},{"battery","Battery"},{"volume","PipeWire volume"},
        {"network","Network interface"},
    };
    for (int i = 0; i < 9; i++) {
        vgfx_text_bold(ctx, wl[i][0], x + 4, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_ACCENT));
        vgfx_text(ctx, wl[i][1], x + w*0.3f, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
        y += LH * 0.8f;
    }
}

static void page_theme(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Theme"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Active:");
    vgfx_text_bold(ctx, S.theme_name, x + 80, y + FS, FS, vgfx_theme_color(ctx, VGP_THEME_ACCENT));
    y += LH + 8;
    desc_text(ctx, &y, x, "Click a theme to switch. Hot-reloaded."); y += 4;
    for (int i = 0; i < S.theme_count; i++) {
        bool sel = strcmp(S.themes[i], S.theme_name) == 0;
        if (vgfx_list_item(ctx, x, y, w, LH + 4, S.themes[i], sel)) {
            snprintf(S.theme_name, 64, "%s", S.themes[i]);
            config_set_value(S.config_path, "general", "theme", S.theme_name);
            mark_changed("Theme applied");
        }
        y += LH + 6;
    }
}

static void page_background(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Background"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Mode");
    if (vgfx_dropdown(ctx, x + w*0.3f, y, w*0.35f, bg_modes, 4, &S.bg_mode, &S.dd_bg_mode))
        mark_changed("Background mode changed");
    y += S.dd_bg_mode ? LH * 6 : LH + 8;
    desc_text(ctx, &y, x, "solid = theme color, shader = GLSL, none = black"); y += 4;
    if (S.bg_mode == 1) {
        if (labeled_input(ctx, &y, x, w, "Shader path", S.bg_shader, 256))
            mark_changed("Shader changed");
    } else if (S.bg_mode == 2) {
        if (labeled_input(ctx, &y, x, w, "Wallpaper", S.bg_wallpaper, 256))
            mark_changed("Wallpaper changed");
    }
}

static void page_keybinds(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Keybinds"); y += LH * 1.5f;
    desc_text(ctx, &y, x, "Click key combo to re-bind. Escape cancels."); y += 4;
    vgfx_text_bold(ctx, "KEY COMBO", x, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    vgfx_text_bold(ctx, "ACTION", x + w * 0.45f, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    y += LH;
    vgfx_separator(ctx, x, y, w); y += 4;

    float row_h = LH;
    int visible = (int)((ctx->height - y - 30) / row_h);
    for (int i = ctx->scroll_offset; i < S.keybind_count && (i - ctx->scroll_offset) < visible; i++) {
        float ry = y + (float)(i - ctx->scroll_offset) * row_h;
        /* Key combo as text input area (simplified) */
        vgfx_text(ctx, S.keybinds[i].key, x, ry + FS, FS, vgfx_theme_color(ctx, VGP_THEME_FG));
        vgfx_text(ctx, S.keybinds[i].action, x + w * 0.45f, ry + FS, FS,
                    vgfx_theme_color(ctx, VGP_THEME_ACCENT));
    }
    if (S.keybind_count > visible)
        vgfx_scrollbar(ctx, x + w - 8, y, (float)visible * row_h, visible, S.keybind_count, &ctx->scroll_offset);
}

static void page_monitors(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Monitor Layout"); y += LH * 1.5f;
    if (S.monitor_count == 0) {
        vgfx_label(ctx, x, y, "No monitors configured.");
        return;
    }
    for (int i = 0; i < S.monitor_count; i++) {
        char hdr[32]; snprintf(hdr, 32, "Monitor %d", i);
        vgfx_section(ctx, x, y, w, hdr); y += LH * 1.2f;
        float mx = (float)S.monitors[i].x;
        vgfx_label(ctx, x, y, "X position");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &mx, 0, 10000, "%.0f"))
            { S.monitors[i].x = (int)mx; mark_changed("Monitor X changed"); }
        y += LH;
        float my = (float)S.monitors[i].y;
        vgfx_label(ctx, x, y, "Y position");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &my, 0, 5000, "%.0f"))
            { S.monitors[i].y = (int)my; mark_changed("Monitor Y changed"); }
        y += LH;
        float mw = (float)S.monitors[i].workspace;
        vgfx_label(ctx, x, y, "Workspace");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &mw, 0, 8, "%.0f"))
            { S.monitors[i].workspace = (int)mw; mark_changed("Monitor workspace changed"); }
        y += LH + 8;
    }
}

static void page_autostart(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Autostart Programs"); y += LH * 1.5f;
    desc_text(ctx, &y, x, "Launched automatically when VGP starts."); y += 4;
    for (int i = 0; i < S.autostart_count; i++) {
        char lbl[8]; snprintf(lbl, 8, "%d.", i+1);
        vgfx_label(ctx, x, y, lbl);
        if (vgfx_text_input(ctx, x + 30, y, w - 30, S.autostart[i], 256))
            mark_changed("Autostart changed");
        y += LH + 4;
    }
    if (S.autostart_count == 0) vgfx_label(ctx, x, y, "No autostart programs.");
}

static void page_rules(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Window Rules"); y += LH * 1.5f;
    if (S.rule_count == 0) {
        vgfx_label(ctx, x, y, "No window rules configured.");
        return;
    }
    vgfx_text_bold(ctx, "TITLE", x, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    vgfx_text_bold(ctx, "FLOAT", x + w*0.6f, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    vgfx_text_bold(ctx, "WS", x + w*0.8f, y + FS, FS - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    y += LH; vgfx_separator(ctx, x, y, w); y += 4;
    for (int i = 0; i < S.rule_count; i++) {
        vgfx_text(ctx, S.rules[i].title, x, y + FS, FS, vgfx_theme_color(ctx, VGP_THEME_FG));
        if (vgfx_checkbox(ctx, x + w*0.6f, y, "", &S.rules[i].floating))
            mark_changed("Rule float toggled");
        char ws[8]; snprintf(ws, 8, "%d", S.rules[i].workspace);
        vgfx_text(ctx, ws, x + w*0.8f, y + FS, FS, vgfx_theme_color(ctx, VGP_THEME_ACCENT));
        y += LH;
    }
}

static void page_lockscreen(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Lock Screen"); y += LH * 1.5f;
    if (vgfx_checkbox(ctx, x, y, "Enable lock screen", &S.lock_enabled))
        mark_changed("Lock screen toggled");
    y += LH + 4;
    desc_text(ctx, &y, x, "Activates after idle timeout. Authenticates via PAM."); y += 4;
    if (S.lock_enabled) {
        float t = (float)S.lock_timeout;
        vgfx_label(ctx, x, y, "Idle timeout");
        if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &t, 1, 30, "%.0f min"))
            { S.lock_timeout = (int)t; mark_changed("Lock timeout changed"); }
    }
}

static void page_accessibility(vgfx_ctx_t *ctx, float y, float x, float w)
{
    vgfx_section(ctx, x, y, w, "Visual"); y += LH * 1.5f;
    if (vgfx_checkbox(ctx, x, y, "High contrast mode", &S.a11y_high_contrast))
        mark_changed("High contrast toggled");
    y += LH + 4;
    if (vgfx_checkbox(ctx, x, y, "Focus indicator ring", &S.a11y_focus_indicator))
        mark_changed("Focus indicator toggled");
    y += LH + 4;
    if (vgfx_checkbox(ctx, x, y, "Large cursor", &S.a11y_large_cursor))
        mark_changed("Large cursor toggled");
    y += LH + 4;
    vgfx_label(ctx, x, y, "Text size");
    if (vgfx_slider(ctx, x + w*0.3f, y, w*0.7f, &S.a11y_text_size, 0, 32, "%.0f pt"))
        mark_changed("Text size changed");
    y += LH + 12;
    vgfx_section(ctx, x, y, w, "Motion"); y += LH * 1.5f;
    if (vgfx_checkbox(ctx, x, y, "Reduce animations", &S.a11y_reduce_anims))
        mark_changed("Reduce animations toggled");
}

static void page_about(vgfx_ctx_t *ctx, float y, float x, float w)
{
    (void)w;
    y += LH;
    vgfx_heading(ctx, x, y, "VGP"); y += LH * 1.5f;
    vgfx_label(ctx, x, y, "Vector Graphics Protocol"); y += LH;
    vgfx_label_colored(ctx, x, y, "Version 0.1.0", vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY)); y += LH;
    vgfx_label(ctx, x, y, "GPU-accelerated vector display server for Linux"); y += LH * 1.5f;
    vgfx_label_colored(ctx, x, y, "Everything is rendered as vectors.", vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY)); y += LH;
    vgfx_label_colored(ctx, x, y, "No X11. No Wayland. Pure TUI desktop.", vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY)); y += LH * 1.5f;
    vgfx_label_colored(ctx, x, y, "https://github.com/theesfeld/VGP", vgfx_theme_color(ctx, VGP_THEME_ACCENT)); y += LH * 2;

    vgfx_section(ctx, x, y, w, "Quick Reference"); y += LH * 1.2f;
    const char *keys[][2] = {
        {"Super+Return","Terminal"},{"Super+D","Launcher"},{"Super+Q","Close"},
        {"Super+Space","Toggle float"},{"Super+L","Lock"},{"Alt+Tab","Cycle focus"},
        {"Super+1-9","Workspace"},{"Ctrl+Alt+BkSp","Quit VGP"},
    };
    for (int i = 0; i < 8; i++) {
        vgfx_text_bold(ctx, keys[i][0], x, y + FS, FS, vgfx_theme_color(ctx, VGP_THEME_FG));
        vgfx_text(ctx, keys[i][1], x + w*0.4f, y + FS, FS, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
        y += LH;
    }
}

/* ============================================================
 * Main render with sidebar
 * ============================================================ */

static void render(vgfx_ctx_t *ctx)
{
    hud_palette_t HP = hud_palette();
    vgfx_clear(ctx, vgfx_rgba(0, 0, 0, 0));

    float w = ctx->width, h = ctx->height;

    /* === DED-STYLE HEADER === */
    float hdr_h = 22.0f;
    vgfx_rect(ctx, 0, 0, w, hdr_h, HP.shade);
    vgfx_line(ctx, 0, hdr_h, w, hdr_h, 1.0f, HP.dim);
    hud_etched_bold(ctx, "DED-SETTINGS", 10, 15, 12, &HP);
    if (S.unsaved) {
        const char *msg = "*UNSAVED*";
        float tw = vgfx_text_width(ctx, msg, -1, 11);
        vgfx_text_bold(ctx, msg, w - tw - 10, 15, 11, HP.warn);
    }

    /* === LEFT OSB COLUMN (page selection) === */
    float sb_w = 140.0f;
    float sb_top = hdr_h + 4.0f;
    float foot_h = 28.0f;
    float sb_bot = h - foot_h - 4.0f;
    float btn_h = (sb_bot - sb_top) / (float)PAGE_COUNT - 2.0f;
    if (btn_h < 22.0f) btn_h = 22.0f;

    for (int i = 0; i < PAGE_COUNT; i++) {
        bool active = ((int)S.current_page == i);
        float by = sb_top + (float)i * (btn_h + 2.0f);
        hud_osb_t osb = { page_labels[i] + 2, active, true };
        if (hud_osb_draw(ctx, 4, by, sb_w - 8, btn_h, &osb, &HP)) {
            S.current_page = i;
            S.edit_field = -1;
            ctx->scroll_offset = 0;
        }
    }

    /* === CONTENT FRAME === */
    float cx = sb_w + P;
    float cy = sb_top;
    float cw = w - cx - P;
    float chh = sb_bot - sb_top;
    vgfx_rect_outline(ctx, cx, cy, cw, chh, 1.0f, HP.dim);

    /* Page title strip */
    float title_h = 20.0f;
    vgfx_rect(ctx, cx, cy, cw, title_h, HP.shade);
    vgfx_line(ctx, cx, cy + title_h, cx + cw, cy + title_h, 1.0f, HP.dim);
    char ptit[64];
    snprintf(ptit, sizeof(ptit), "PAGE: %s", page_labels[S.current_page] + 2);
    hud_etched_bold(ctx, ptit, cx + 10, cy + title_h * 0.5f + 11 * 0.35f, 11, &HP);

    typedef void (*page_fn)(vgfx_ctx_t*, float, float, float);
    page_fn pages[] = {
        page_general, page_input, page_windows, page_panel,
        page_theme, page_background, page_keybinds, page_monitors,
        page_autostart, page_rules, page_lockscreen, page_accessibility,
        page_about,
    };
    if (S.current_page < PAGE_COUNT)
        pages[S.current_page](ctx, cy + title_h + 6, cx + 8, cw - 16);

    /* === BOTTOM OSB ROW === */
    float fy = h - foot_h + 2.0f;
    hud_osb_t foot_osb[] = {
        { "SAVE",   false, S.unsaved },
        { "RELOAD", false, true },
        { "QUIT",   false, true },
    };
    int fc = 3;
    float fbw = 110.0f;
    for (int i = 0; i < fc; i++) {
        float fbx = 4.0f + (float)i * (fbw + 4.0f);
        if (hud_osb_draw(ctx, fbx, fy, fbw, foot_h - 6, &foot_osb[i], &HP)) {
            if (i == 0) save_config();
            else if (i == 1) load_config();
            else if (i == 2) ctx->running = false;
        }
    }

    /* Status message (right of buttons) */
    float sx = 4.0f + (float)fc * (fbw + 4.0f) + 8.0f;
    if (S.status_timer > 0) {
        vgfx_text(ctx, S.status, sx, h - 10, FS - 2, HP.warn);
        S.status_timer--;
    } else {
        hud_etched(ctx, "ESC closes window", sx, h - 10, FS - 2, &HP);
    }

    if (ctx->key_pressed && ctx->last_keysym == 0xFF1B && ctx->focus_id == 0)
        ctx->running = false;
}

#undef LH
#undef FS
#undef P

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    char log_path[512];
    if (!vgp_xdg_resolve(VGP_XDG_STATE, "vgp/vgp-settings.log",
                           log_path, sizeof(log_path)))
        snprintf(log_path, sizeof(log_path), "/tmp/vgp-settings.log");
    FILE *lf = fopen(log_path, "w");
    if (lf) { setvbuf(lf, NULL, _IOLBF, 0); dup2(fileno(lf), STDERR_FILENO); fclose(lf); }

    load_config();
    scan_themes();

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Settings", 900, 650, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
