#ifndef VGP_CONFIG_H
#define VGP_CONFIG_H

#include "theme.h"
#include "loop.h"
#include "vgp/types.h"
#include <stdbool.h>
#include <stdint.h>

#define VGP_CONFIG_MAX_PATH         512
#define VGP_CONFIG_MAX_KEYBINDS     128
#define VGP_CONFIG_MAX_WINDOW_RULES 32

typedef struct vgp_window_rule {
    char  title_match[128];
    bool  floating;
    int   workspace;   /* -1 = default */
    int   width, height; /* 0 = default */
} vgp_window_rule_t;

typedef struct vgp_config_general {
    char  terminal_cmd[VGP_CONFIG_MAX_PATH];
    char  launcher_cmd[VGP_CONFIG_MAX_PATH];
    char  font_path[VGP_CONFIG_MAX_PATH];
    char  screenshot_dir[VGP_CONFIG_MAX_PATH];
    char  theme_name[64];              /* theme directory name */
    char  theme_dir[VGP_CONFIG_MAX_PATH]; /* resolved theme directory path */
    float font_size;
    bool  focus_follows_mouse;
    int   workspace_count;
    char  wm_mode[16];              /* "floating", "tiling", "hybrid" */
    char  tile_algorithm[32];       /* "golden_ratio", "equal", "master_stack", "spiral" */
    float tile_master_ratio;        /* master window ratio for master_stack (0.0-1.0) */
    int   tile_gap_inner;           /* pixels between tiled windows */
    int   tile_gap_outer;           /* pixels between tiled windows and screen edge */
    bool  tile_smart_gaps;          /* hide gaps when only 1 window */
} vgp_config_general_t;

typedef struct vgp_config_input {
    float pointer_speed;
    bool  natural_scrolling;
    bool  tap_to_click;
    int   repeat_delay_ms;
    int   repeat_rate_ms;
} vgp_config_input_t;

typedef struct vgp_config_monitor {
    bool     configured;            /* true if user provided config for this monitor */
    int32_t  x, y;                  /* position in layout (-1 = auto) */
    int      workspace;             /* workspace to display (-1 = auto, same as index) */
    float    scale;                 /* output scale (1.0 = native) */
    char     mode[32];              /* "WIDTHxHEIGHT@RATE" or empty for preferred */
} vgp_config_monitor_t;

#define VGP_PANEL_MAX_WIDGETS 8

typedef struct vgp_config_panel {
    char  position[16];          /* "top" or "bottom" */
    int   height;
    char  left_widgets[VGP_PANEL_MAX_WIDGETS][32];   /* widget names */
    int   left_count;
    char  center_widgets[VGP_PANEL_MAX_WIDGETS][32];
    int   center_count;
    char  right_widgets[VGP_PANEL_MAX_WIDGETS][32];
    int   right_count;
} vgp_config_panel_t;

#define VGP_CONFIG_MAX_AUTOSTART 16

typedef struct vgp_config_lockscreen {
    bool  enabled;
    int   timeout_min;
} vgp_config_lockscreen_t;

typedef struct vgp_config_session {
    char  autostart[VGP_CONFIG_MAX_AUTOSTART][256];
    int   autostart_count;
} vgp_config_session_t;

typedef struct vgp_keybind_entry {
    char key_str[64];
    char action_str[256];
} vgp_keybind_entry_t;

typedef struct vgp_config {
    vgp_config_general_t  general;
    vgp_config_input_t    input;
    vgp_theme_t           theme;

    vgp_config_panel_t    panel;
    vgp_window_rule_t     window_rules[VGP_CONFIG_MAX_WINDOW_RULES];
    int                   window_rule_count;
    vgp_config_lockscreen_t lockscreen;
    vgp_config_session_t session;
    vgp_config_monitor_t  monitors[VGP_MAX_OUTPUTS];
    int                   monitor_count;

    vgp_keybind_entry_t   keybind_entries[VGP_CONFIG_MAX_KEYBINDS];
    int                   keybind_count;

    char                  config_path[VGP_CONFIG_MAX_PATH];
} vgp_config_t;

void vgp_config_load_defaults(vgp_config_t *config);
int  vgp_config_load(vgp_config_t *config, const char *path);

#endif /* VGP_CONFIG_H */
