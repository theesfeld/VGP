# VGP - Vector Graphics Protocol

A GPU-accelerated vector display server and desktop environment for Linux. Every pixel is vector-rendered. No X11. No Wayland. Pure VGP.

## What is VGP?

VGP is a display server that renders everything -- windows, text, decorations, cursors, UI elements -- as vector graphics on the GPU using NanoVG and OpenGL ES 3.0. It runs directly on DRM/KMS with no dependency on X11 or Wayland.

The terminal emulator sends a cell grid (character + color + attributes per cell) over IPC, and the server renders the text at native resolution using GPU vector rendering. Resize a terminal to any size -- text is always crisp because it's never a bitmap.

## Features

- **GPU vector rendering** via NanoVG + OpenGL ES 3.0
- **Multi-monitor** with per-monitor workspaces
- **GLSL shader backgrounds** -- cyberpunk grids, parallax, mouse-reactive lighting, window shadow casting
- **Configurable everything** -- keybinds, themes, panel widgets, monitor layout, pointer speed
- **Three default themes** -- Dark (cyberpunk), NERV (tactical HUD), Light (clean professional)
- **Window management** -- floating, snap to edges, maximize, minimize, expose/overview mode
- **Terminal emulator** (vgp-term) with selection, clipboard, scrollback, 256-color + true color
- **Application launcher** with fuzzy search of .desktop files
- **Native apps** -- settings editor, file manager, system monitor, image viewer
- **D-Bus notifications** -- standard freedesktop.org notification daemon
- **Session management** via libseat (runs without root)

## Architecture

```
┌─────────────────────────────────────────────┐
│             VGP Server (964KB)              │
│  DRM/KMS │ NanoVG/GLES3 │ libinput │ D-Bus │
│  libseat │ Compositor   │ IPC      │ Shaders│
└──────────────────┬──────────────────────────┘
                   │ Unix socket (cell grid protocol)
    ┌──────────────┼──────────────┐
    │              │              │
 vgp-term    vgp-settings   vgp-files
 (72KB)       (50KB)         (44KB)
```

Client apps send vector draw commands (cell grids) -- the server renders everything on the GPU. No pixel buffers for text content. A terminal frame is 23KB of cell data vs 1.6MB of pixels.

## Building

### Dependencies

```
libdrm libinput xkbcommon libseat dbus-1 gbm egl glesv2 libvterm
```

On Arch/CachyOS:
```bash
pacman -S meson libdrm libinput libxkbcommon seatd dbus libvterm mesa
```

### Build

```bash
git clone https://github.com/YOUR_USERNAME/VGP.git
cd VGP
meson setup build
meson compile -C build
```

### Run

```bash
# From a TTY (Ctrl+Alt+F2), or from a greeter:
./build/vgp --config ~/.config/vgp/config.toml
```

## Configuration

All config lives in `~/.config/vgp/`:

```
~/.config/vgp/
├── config.toml          # main config (keybinds, input, monitors, panel)
├── terminal.toml        # terminal settings (font, scrollback, cursor, colors)
├── shaders/             # user shader effects (.frag files)
│   ├── background.frag
│   └── panel.frag
└── themes/              # theme directories
    ├── dark/
    │   ├── theme.toml
    │   └── shaders/
    ├── nerv/
    └── light/
```

### Default Keybinds

| Key | Action |
|-----|--------|
| Super+Return | Open terminal |
| Super+D | Open launcher |
| Super+Q | Close window |
| Super+Tab | Expose/overview |
| Super+Left/Right/Up/Down | Snap window |
| Super+1/2/3 | Switch workspace |
| Super+Shift+1/2/3 | Move window to workspace |
| Alt+Tab | Cycle focus |
| Super+S | Settings |
| Super+E | File manager |
| Super+P | System monitor |
| PrintScreen | Screenshot |
| Ctrl+Alt+Backspace | Quit |

## Themes

Themes are self-contained directories with a `theme.toml` and optional GLSL shaders. They control every visual aspect: colors, geometry, fonts, window decorations, panel, cursor, opacity, and shader effects.

Switch themes in `config.toml`:
```toml
[general]
theme = "nerv"
```

## License

MIT
