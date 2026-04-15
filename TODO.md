# VGP Development Roadmap

## Completed

- [x] DRM/KMS + GPU rendering (NanoVG/GLES3)
- [x] libinput + xkbcommon input, libseat session management
- [x] Floating window management (move, resize, snap, maximize, minimize)
- [x] Multi-monitor + per-monitor workspaces
- [x] IPC (Unix socket, cell grid protocol, pixel surfaces)
- [x] Configurable keybinds + TOML config + settings GUI with write-back
- [x] Application launcher (fuzzy .desktop search)
- [x] GLSL shader backgrounds (parallax, shadows, mouse reactive)
- [x] Taskbar + workspace indicators + clock (configurable widgets)
- [x] D-Bus notification daemon
- [x] Clipboard, screenshots, selection, scrollback
- [x] Terminal emulator (vector cell grid, server-side rendering)
- [x] Window animations, drop shadows, window opacity
- [x] Context-sensitive cursor shapes, expose/overview mode
- [x] Native apps: settings, files, monitor, image viewer
- [x] Lock screen (idle timeout, password, themed)
- [x] Session management (auto-start)
- [x] Three themes (dark, nerv, light)
- [x] AUR PKGBUILD, .desktop files, greeter session

---

## In Progress

### Tiling Window Manager
- [ ] Tiling layout engine (user selectable algorithm)
- [ ] Algorithms: golden ratio, equal split, master+stack, spiral
- [ ] wm_mode config: floating, tiling, hybrid
- [ ] Per-workspace mode (some tile, some float)
- [ ] Configurable gaps (inner + outer)
- [ ] Toggle float per window (Super+Space)
- [ ] Resize splits with mouse or keybind
- [ ] Tiling direction (horizontal/vertical split)

### CI/CD + Packaging
- [ ] GitHub Actions: build on tag push, create release tarball
- [ ] GitHub Actions: auto-submit PKGBUILD to AUR
- [ ] AUR SSH key for automated publishing
- [ ] Release versioning (git tag)
- [ ] Test PKGBUILD end-to-end

### Wiki / Documentation
- [ ] What VGP Is / What VGP Is Not
- [ ] Architecture overview (server, protocol, GPU pipeline)
- [ ] Installation: AUR (yay -S vgp-git)
- [ ] Installation: source build (Arch, Fedora, Debian/Ubuntu, NixOS, Void, Gentoo)
- [ ] Dependencies per distro (package name mapping)
- [ ] Configuration reference (config.toml every option)
- [ ] Terminal configuration reference (terminal.toml)
- [ ] Theme creation guide (theme.toml + shaders)
- [ ] Shader development guide (uniforms, examples)
- [ ] Building native apps (libvgp + libvgp-ui SDK)
- [ ] Keybind reference
- [ ] FAQ

---

## Remaining

### Polish
- [ ] Window rules (per-app: float, workspace, size)
- [ ] Panel hover effects, middle-click close
- [ ] Right-click context menu
- [ ] Panel widget rendering from config
- [ ] Monospace grid precision (zero ANSI gaps)
- [ ] Terminal font switching (Ctrl+Plus/Minus)
- [ ] Terminal URL detection
- [ ] Terminal search (Ctrl+Shift+F)

### Theme System
- [ ] NERV theme: angular decorations, warning stripes
- [ ] Light theme: complete styling
- [ ] Dark/light toggle (Super+Shift+T)
- [ ] Theme hot-reload (inotify)
- [ ] Shader library (aurora, matrix, gradient)
- [ ] Calendar popup
- [ ] Theme packaging (.vgptheme)
- [ ] Theme/shader browser in settings

### Visual Effects
- [ ] Blur behind transparent windows
- [ ] Workspace slide animation
- [ ] Taskbar window preview on hover
- [ ] Pluggable notification animations
- [ ] Pluggable window animations (wobbly, flame)

### Apps
- [ ] vgp-edit: text editor + syntax highlighting
- [ ] vgp-bar: standalone scriptable bar
- [ ] Settings: color picker, shader previews
- [ ] Client SDK docs

### Production
- [ ] Drag and drop
- [ ] Monitor arrangement GUI
- [ ] Session persistence (save/restore layout)
- [ ] Power management (DPMS, suspend)
- [ ] IPC control (swaymsg-style scripting)
- [ ] Accessibility
