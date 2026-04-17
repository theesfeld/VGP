# Configuration

All VGP configuration lives in `~/.config/vgp/`. The main config file is `config.toml`.

## config.toml Reference

```toml
[general]
terminal = "vgp-term"           # default terminal command
launcher = "vgp-launcher"       # default launcher command
workspaces = 9                  # number of workspaces
screenshot_dir = "~/Pictures"   # screenshot save directory
theme = "dark"                  # active theme (dark, nerv, light, or custom)
wm_mode = "tiling"              # floating, tiling, or hybrid
tile_algorithm = "golden_ratio" # golden_ratio, equal, master_stack, spiral
tile_master_ratio = 0.55        # master window ratio (master_stack only)
tile_gap_inner = 6              # pixels between tiled windows
tile_gap_outer = 8              # pixels between windows and screen edge
tile_smart_gaps = true          # hide gaps with single window

[input]
pointer_speed = 3.0             # mouse speed multiplier
natural_scrolling = false
tap_to_click = true
repeat_delay = 300              # key repeat delay (ms)
repeat_rate = 30                # key repeat rate (ms)

[keybinds]
Super+Return = "spawn_terminal"
Super+d = "spawn_launcher"
Super+q = "close_window"
Super+m = "maximize_window"
Super+n = "minimize_window"
Super+f = "fullscreen"
Super+Tab = "expose"
Super+l = "lock"
Super+space = "toggle_float"
Alt+Tab = "focus_next"
Alt+Shift+Tab = "focus_prev"
Super+Left = "snap_left"
Super+Right = "snap_right"
Super+Up = "snap_top"
Super+Down = "snap_bottom"
Super+1 = "workspace_1"
Super+Shift+1 = "move_to_workspace_1"
Print = "screenshot"
Ctrl+Alt+BackSpace = "quit"
Super+e = "exec:vgp-files"     # custom command

[panel]
position = "bottom"             # top or bottom
height = 32                     # pixels

[panel.widgets.left]
items = "workspaces, settings"

[panel.widgets.center]
items = "taskbar"

[panel.widgets.right]
items = "monitor, clock"

[lockscreen]
enabled = true
timeout = 5                     # minutes of idle before lock

[session]
autostart = "vgp-term"          # launched on startup (multiple entries allowed)

[monitor.0]
x = 0
y = 0
workspace = 0

[monitor.1]
x = 2880
y = 0
workspace = 1

[theme]
# Override theme colors (or use a theme directory)
background = "#1A1A2E"
border_active = "#5294E2"
```

## terminal.toml Reference

```toml
[general]
shell = ""                      # empty = $SHELL or /bin/sh
scrollback = 10000
font_size = 14.0
cursor_style = "block"          # block, underline, bar
cursor_blink = true
padding = 4

[keybinds]
copy = "Ctrl+Shift+c"
paste = "Ctrl+Shift+v"
scroll_up = "Shift+Page_Up"
scroll_down = "Shift+Page_Down"
```

## Available Panel Widgets

| Widget | Description |
|--------|-------------|
| workspaces | Clickable workspace indicators |
| taskbar | Window list (click to focus) |
| clock | Current time |
| date | Current date |
| settings | Settings app button |
| monitor | CPU/RAM mini-display |
| files | File manager button |
| launcher | App launcher button |
| battery | Battery status |
| volume | Volume control |

## Tiling Algorithms

| Algorithm | Description |
|-----------|-------------|
| golden_ratio | Recursive phi (1.618) splits, alternating horizontal/vertical |
| equal | Grid of equal-sized windows |
| master_stack | One large master window, rest stacked vertically |
| spiral | Fibonacci inward spiral |

---

← **[Installation](Installation)** · Next: **[Themes](Themes)** →
