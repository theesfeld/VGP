# FAQ

## Why not X11 or Wayland?

VGP is a purpose-built display server for a specific use case: GPU-accelerated vector rendering of terminal/TUI applications. X11 and Wayland are general-purpose protocols designed for raster (pixel-based) GUI applications. VGP's protocol sends vector draw commands (cell grids with characters and colors), not pixel buffers. This architectural difference enables:

- **Always-crisp text** at any window size (no bitmap scaling)
- **70× less IPC data** for terminal content (23 KB vs 1.6 MB per frame)
- **Custom GLSL shaders** on every UI element
- **Compact C17 codebase** — roughly 40 k SLOC

## Can I run Firefox / Chrome / GUI apps?

Not currently. VGP has its own protocol (`libvgp`) that external GUI
applications don't speak. TUI / CLI programs run inside `vgp-term` just
like any other terminal. There is no X11 or Wayland bridge in the tree
today; an XWayland-style shim could be built against the `libvgp`
draw-command channel but none exists.

## What GPU do I need?

Any GPU with OpenGL ES 3.0 support via Mesa. This includes:
- Intel HD 4000+ (Ivy Bridge and newer)
- AMD GCN 1.0+ (HD 7000 series and newer)
- NVIDIA with Nouveau or proprietary drivers

VGP also has a CPU fallback (plutovg software renderer) for systems without GPU support, but performance will be limited on high-resolution displays.

## Does VGP support multiple monitors?

Yes. Each monitor gets its own workspace. Monitors are laid out side-by-side by default. Configure positions in `[monitor.N]` sections of `config.toml`.

## How is VGP different from a terminal multiplexer (tmux/screen)?

VGP is a full display server with:
- Hardware GPU acceleration
- Mouse support (click, drag, scroll, selection)
- Window decorations (titlebars, buttons, borders)
- Multiple overlapping windows
- Taskbar, notifications, lock screen
- Shader visual effects
- Theme system

Terminal multiplexers run inside a single terminal. VGP IS the terminal.

## Can I use my existing dotfiles?

Your shell config (`.bashrc`, `.zshrc`, etc.) works as-is inside `vgp-term`. VGP has its own config format (`config.toml`) for the window manager and display server settings.

## Is this ready for daily use?

VGP is in active development. It's functional but may have rough edges. Use it alongside your existing DE/WM (switch between them at the greeter) as you evaluate it.

---

← **[Architecture](Architecture)** · Back to **[Home](Home)**
