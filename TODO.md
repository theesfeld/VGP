# VGP Development Roadmap

## Done (15,000+ lines of C)

Server: DRM/KMS, NanoVG/GLES3, libinput, xkbcommon, libseat, D-Bus notifications, GLSL shaders, multi-monitor workspaces, cell grid protocol, IPC control socket, session save, power management, lock screen, window animations, drop shadows, opacity, tiling (4 algorithms), expose mode, cursor themes, context menu

Terminal: vector cell grid, server-side rendering, selection, clipboard, scrollback, bracketed paste, configurable

Apps: launcher, settings (editable + write-back), files, monitor, image viewer, shared UI toolkit

Config: TOML, themes, shaders, keybinds, panel widgets, window rules, tiling, lock screen, auto-start, monitors

Infra: PKGBUILD, .desktop files, greeter session, GitHub Actions, wiki (7 pages)

---

## What's Left

### Must-Have (before v1.0)
- [ ] Panel widget rendering driven by config (currently hardcoded in renderer)
- [ ] Terminal font size change at runtime (Ctrl+Plus/Minus)
- [ ] Theme hot-reload (inotify watch on theme.toml + shaders)
- [ ] Dark/light mode toggle keybind
- [ ] Desktop menu actions wired up (currently menu items have NULL callbacks)
- [ ] Right-click on window titlebar for window-specific menu (close, minimize, maximize, float, move to workspace)
- [ ] Resize tiling splits with mouse drag
- [ ] CI/CD: test the release workflow end-to-end (tag + build + AUR)
- [ ] Panel hover effects on taskbar entries

### Should-Have (v1.1)
- [ ] Blur behind transparent windows (FBO + gaussian blur shader)
- [ ] Smooth workspace slide animation
- [ ] Taskbar window preview on hover
- [ ] Terminal URL detection + clickable links
- [ ] Terminal search (Ctrl+Shift+F)
- [ ] Calendar popup when clicking clock
- [ ] Monospace grid precision (zero sub-pixel gaps between ANSI blocks)
- [ ] NERV theme fully styled (angular decorations, warning stripes)
- [ ] Light theme fully styled
- [ ] Shader library (aurora, matrix rain, gradient, more backgrounds)
- [ ] Theme packaging (.vgptheme zip format)
- [ ] Pluggable window/notification animations

### Nice-to-Have (v1.2+)
- [ ] vgp-edit: text editor with syntax highlighting
- [ ] vgp-bar: standalone scriptable bar (polybar replacement)
- [ ] Settings GUI: color picker widget
- [ ] Settings GUI: shader effect live preview
- [ ] Monitor arrangement GUI (drag to position)
- [ ] Drag and drop between windows
- [ ] Session layout restore (reopen windows at saved positions)
- [ ] Accessibility (keyboard-only nav, high contrast, screen reader hooks)
- [ ] Client SDK documentation
- [ ] DPMS proper implementation (DRM connector properties)
