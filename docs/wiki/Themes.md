# Themes

Themes are self-contained directories that define every visual aspect of VGP.

## Theme Directory Structure

```
~/.config/vgp/themes/my-theme/
├── theme.toml          # complete UI definition
└── shaders/            # GLSL fragment shaders
    ├── background.frag
    └── panel.frag
```

## Switching Themes

In `config.toml`:
```toml
[general]
theme = "dark"    # or "nerv", "light", or any directory name
```

Or use the Settings app (Super+S) → Theme tab.

## Default Themes

| Theme | Description |
|-------|-------------|
| **dark** | Modern dark theme with cyberpunk shader background |
| **nerv** | Tactical HUD inspired by NERV/Evangelion -- orange, angular, radar sweep |
| **light** | Clean professional theme, no shader background |

## Creating a Theme

Create a directory in `~/.config/vgp/themes/` with a `theme.toml`:

```toml
[meta]
name = "My Theme"
author = "Your Name"
version = "1.0"

[colors]
background = "#1A1A2E"
foreground = "#E0E0E0"
accent = "#5294E2"
surface = "#1E1E2E"
border = "#3C3C4C"
error = "#E06060"
success = "#60C060"
warning = "#E0C040"

[window]
titlebar_height = 32
border_width = 2
corner_radius = 8
opacity = 0.9
inactive_opacity = 0.85

[window.active]
titlebar_bg = "#2D2D2D"
border_color = "#5294E2"

[window.inactive]
titlebar_bg = "#1E1E1E"
border_color = "#3C3C3C"

[window.buttons]
radius = 7
close = "#E06060"
maximize = "#60C060"
minimize = "#E0C040"

[panel]
bg = "#16213E"
text = "#C0C0C0"
height = 32

[shaders]
background = "shaders/background.frag"

[background]
mode = "shader"       # solid, shader, wallpaper, none
```

## Packaging Themes

A theme is just a directory. To share it:
1. Zip the directory: `zip -r mytheme.zip mytheme/`
2. Others extract to `$XDG_CONFIG_HOME/vgp/themes/`
3. Select in config or settings GUI

---

← **[Configuration](Configuration)** · Next: **[Shaders](Shaders)** →
