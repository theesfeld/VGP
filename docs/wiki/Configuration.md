# Configuration

## File locations

VGP follows the [XDG Base Directory Specification][xdg]. The main config
lives at:

```
$XDG_CONFIG_HOME/vgp/config.toml        # typically ~/.config/vgp/config.toml
```

When `XDG_CONFIG_HOME` is unset it defaults to `$HOME/.config`. VGP will
also read from `$XDG_CONFIG_DIRS/vgp/config.toml` (default `/etc/xdg`) if a
user file is missing — so system administrators can ship a site-wide
default.

Related paths:

| Path                                                 | Purpose                                  |
| ---                                                  | ---                                      |
| `$XDG_CONFIG_HOME/vgp/config.toml`                   | Main config                              |
| `$XDG_CONFIG_HOME/vgp/terminal.toml`                 | Terminal-emulator overrides              |
| `$XDG_CONFIG_HOME/vgp/themes/<name>/theme.toml`      | User theme                               |
| `$XDG_DATA_HOME/vgp/themes/...`                      | User data-area themes                    |
| `$XDG_DATA_DIRS/vgp/themes/...`                      | System themes (e.g. `/usr/share/vgp/...`) |
| `$XDG_CONFIG_HOME/vgp/shaders/<name>.frag`           | User shaders                             |
| `$XDG_CONFIG_HOME/autostart/*.desktop`               | [XDG Autostart][xdg-auto] entries        |
| `$XDG_STATE_HOME/vgp/vgp.log`                        | Compositor log                           |
| `$XDG_STATE_HOME/vgp/session.json`                   | Saved window layout                      |
| `$XDG_STATE_HOME/vgp/launcher_history`               | Launcher usage counts                    |

## config.toml reference

Every key below is what the current parser in `src/server/config.c`
accepts. Unknown keys are silently ignored.

### `[general]`

```toml
[general]
terminal = "vgp-term"                  # Super+Return spawns this
launcher = "vgp-launcher"              # Super+D spawns this
theme = "hud"                          # Theme directory name (see Themes)
url_handler = "vgp-term -e w3m '%s'"   # %s is replaced with the URL
screenshot_dir = "/home/user/Pictures" # honours $XDG_PICTURES_DIR by default
font = ""                              # Custom font file path (empty = default)
font_size = 14.0
focus_follows_mouse = false
workspaces = 3                         # Number of workspaces
wm_mode = "floating"                   # floating | tiling | hybrid
tile_algorithm = "golden_ratio"        # golden_ratio | equal | master_stack | spiral
tile_master_ratio = 0.55               # master_stack only
tile_gap_inner = 6                     # px between tiled windows
tile_gap_outer = 8                     # px between windows and screen edge
tile_smart_gaps = true                 # hide gaps when only one tile is visible
```

### `[input]`

```toml
[input]
pointer_speed     = 3.0     # mouse multiplier
natural_scrolling = false
tap_to_click      = true
repeat_delay      = 300     # ms before key-repeat kicks in
repeat_rate       = 30      # ms between repeated keys
```

### `[keybinds]`

Map a key combo to an action or an `exec:` shell command:

```toml
[keybinds]
Super+Return   = "spawn_terminal"
Super+d        = "spawn_launcher"
Super+q        = "close_window"
Super+m        = "maximize_window"
Super+n        = "minimize_window"
Super+f        = "fullscreen"
Super+space    = "toggle_float"
Super+Tab      = "expose"
Super+l        = "lock"
Alt+Tab        = "focus_next"
Alt+Shift+Tab  = "focus_prev"
Super+Left     = "snap_left"
Super+Right    = "snap_right"
Super+Up       = "snap_top"
Super+Down     = "snap_bottom"
Super+1        = "workspace_1"        # up to workspace_9
Super+Shift+1  = "move_to_workspace_1"  # up to 9
Print          = "screenshot"
Ctrl+Alt+BackSpace = "quit"

# Spawn an arbitrary command by prefixing the value with exec:
Super+s = "exec:vgp-settings"
Super+e = "exec:vgp-files"
Super+p = "exec:vgp-monitor"

# Media keys
XF86AudioRaiseVolume = "volume_up"
XF86AudioLowerVolume = "volume_down"
XF86AudioMute        = "volume_mute"
```

**Every supported action:**

| Action                    | Effect                                    |
| ---                       | ---                                       |
| `spawn_terminal`          | Spawn `general.terminal`                  |
| `spawn_launcher`          | Spawn `general.launcher`                  |
| `close_window`            | Close the focused window                  |
| `maximize_window`         | Toggle maximize on the focused window     |
| `minimize_window`         | Minimize the focused window               |
| `fullscreen`              | Toggle fullscreen on the focused window   |
| `focus_next` / `focus_prev` | Cycle focus                             |
| `workspace_1` … `workspace_9` | Switch active workspace               |
| `move_to_workspace_1` … `move_to_workspace_9` | Move focused window |
| `snap_left` / `snap_right` / `snap_top` / `snap_bottom` | Half-screen snap |
| `expose`                  | Overview / Mission-Control mode           |
| `toggle_float`            | Float / tile the focused window           |
| `toggle_dark_light`       | Toggle between dark and light theme       |
| `lock`                    | Lock the session                          |
| `screenshot`              | Save a PNG to `screenshot_dir`            |
| `volume_up` / `volume_down` / `volume_mute` | via wpctl            |
| `quit`                    | Shut the compositor down                  |
| `exec:<command>`          | Shell-spawn (`/bin/sh -c`)                |

### `[panel]`

```toml
[panel]
position = "top"       # top | bottom
height   = 32          # px

[panel.widgets.left]
items = "workspaces"

[panel.widgets.center]
items = "taskbar"

[panel.widgets.right]
items = "cpu, memory, network, clock, date"
```

**Every implemented widget:**

| Widget        | Renders                                              |
| ---           | ---                                                  |
| `workspaces`  | Clickable workspace indicators (1..n)                |
| `taskbar`     | Window list on the active workspace                  |
| `clock`       | `HH:MM`, click toggles calendar                      |
| `date`        | `MM/DD`, click toggles calendar                      |
| `cpu`         | `CPU NN%` (sampled from `/proc/stat`)                |
| `memory`      | `MEM NN%` (sampled from `/proc/meminfo`)             |
| `battery`     | `BAT [+]NN%` if `/sys/class/power_supply/BAT0`       |
| `volume`      | `VOL NN%` via `wpctl`                                |
| `network`     | Active interface name                                |

Unknown widget names are silently skipped.

### `[lockscreen]`

```toml
[lockscreen]
enabled = true
timeout = 5            # minutes of idle before auto-lock
```

Authentication is PAM-based (`pam_unix` by default).

### `[accessibility]`

```toml
[accessibility]
high_contrast     = false
focus_indicator   = false        # draw a ring around the focused window
large_cursor      = false
reduce_animations = false
text_size         = 0            # 0 = theme default; absolute points otherwise
```

### `[session]`

```toml
[session]
# Each autostart line is spawned at session start.
autostart = "vgp-bar"
autostart = "pasystray"
```

In addition to this list, every `.desktop` entry in
`$XDG_CONFIG_HOME/autostart` (and each `$XDG_CONFIG_DIRS/autostart`) is
launched per the [XDG Autostart Spec][xdg-auto]. `Hidden=true` entries are
skipped; duplicate filenames across directories are deduped with the
higher-priority directory winning.

### `[monitor.N]`

Zero-indexed per-output layout. The compositor lays monitors out
left-to-right in the order meson-reported outputs appear, which is
usually the order `/sys/class/drm/` enumerates them.

```toml
[monitor.0]
x         = 0
y         = 0
workspace = 0       # default workspace shown on this output
scale     = 1.0
mode      = ""      # optional mode string, e.g. "2560x1440@144"

[monitor.1]
x         = 2880
y         = 0
workspace = 1

[monitor.2]
x         = 6720
y         = 0
workspace = 2
```

### `[rule.N]`

Per-window rules, zero-indexed:

```toml
[rule.0]
match     = "firefox"    # case-insensitive substring match against title
floating  = true
workspace = 2
width     = 1600
height    = 900
```

### `[theme]`

Inline overrides — equivalent to editing the theme's `theme.toml`. Any
key from [Themes](Themes) works here:

```toml
[theme]
titlebar_height = 32
corner_radius   = 20
border_active   = "#FFD700"
border_inactive = "#666666"
background      = "#000000"
```

## terminal.toml reference

Overrides for `vgp-term`. Resolved via the XDG search path (user file,
then `$XDG_CONFIG_DIRS/vgp/terminal.toml`).

```toml
[general]
shell          = ""            # empty = $SHELL, else /bin/sh
scrollback     = 10000
font_size      = 14.0
cursor_style   = "block"       # block | underline | bar
cursor_blink   = true
padding        = 4

[keybinds]
copy        = "Ctrl+Shift+c"
paste       = "Ctrl+Shift+v"
scroll_up   = "Shift+Page_Up"
scroll_down = "Shift+Page_Down"
```

## Tiling algorithms

| Algorithm      | Layout                                                    |
| ---            | ---                                                       |
| `golden_ratio` | Recursive phi (1.618) splits, alternating horiz/vert      |
| `equal`        | Grid of equal-sized windows                               |
| `master_stack` | One master window + vertical stack, split by `tile_master_ratio` |
| `spiral`       | Fibonacci inward spiral                                   |

Per-workspace overrides can be set via window rules or by launching apps
with different `wm_mode` on-the-fly through the settings editor.

## Environment variables

The compositor reads these at startup:

| Variable           | Effect                                                |
| ---                | ---                                                   |
| `VGP_CPU=1`        | Force CPU renderer (plutovg), skip the GPU backend    |
| `VGP_FBO=0`        | Disable the FBO glass pipeline                        |
| `XDG_*`            | Honoured per the [XDG Base Dir Spec][xdg]             |

## Live re-load

Saving `config.toml` takes effect on the next SIGHUP:

```bash
pkill -HUP vgp
```

Or use `vgp-settings` — it sends the signal after `SAVE`.

[xdg]:      https://specifications.freedesktop.org/basedir-spec/latest/
[xdg-auto]: https://specifications.freedesktop.org/autostart-spec/latest/

---

← **[Installation](Installation)** · Next: **[Themes](Themes)** →
