# VGP - Vector Graphics Protocol

Welcome to the VGP wiki.

## What VGP Is

VGP is a **GPU-accelerated vector display server** and desktop environment for Linux. It is a complete replacement for X11/Wayland for users who primarily use terminal/TUI applications.

- **Everything is vector-rendered** -- text, windows, decorations, cursors, UI elements are all drawn as vector paths on the GPU via NanoVG and OpenGL ES 3.0
- **No pixel buffers for text** -- the terminal sends a cell grid (character + color per cell), the server renders text at native resolution. Resize a window to any size -- text is always crisp
- **Custom GLSL shader backgrounds** -- animated, parallax, mouse-reactive, window shadow casting
- **Fully configurable** -- every aspect is user-controlled via TOML config files and a built-in settings GUI
- **Themeable** -- themes control everything: colors, geometry, fonts, shaders, decorations
- **Tiling + floating** window management with configurable algorithms

## What VGP Is Not

- **Not an X11 replacement for GUI apps** -- Firefox, Chrome, VS Code, GIMP, etc. will not run natively. VGP has its own protocol. (XWayland support is planned for the future.)
- **Not Wayland** -- VGP is a completely independent protocol. It does not implement the Wayland protocol.
- **Not a compositor for existing apps** -- VGP apps must be built against `libvgp`. The terminal emulator handles TUI/CLI programs.
- **Not production-stable yet** -- VGP is in active development. Use at your own risk.

## Pages

- [[Installation]]
- [[Configuration]]
- [[Themes]]
- [[Shaders]]
- [[Keybinds]]
- [[Building Apps]]
- [[Architecture]]
- [[FAQ]]
