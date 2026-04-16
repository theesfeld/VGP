# VGP Development Roadmap

## Done

### v1.0 — Core
- GPU vector rendering (NanoVG/GLES3), DRM/KMS, multi-monitor
- Tiling + floating WM with configurable algorithms
- Terminal emulator (cell grid protocol, scrollback, selection, clipboard)
- Apps: launcher, settings, files, monitor, editor, image viewer, bar
- Config system: themes, shaders, keybinds, panel widgets, window rules
- Lock screen (PAM), IPC control socket, context menus
- CI/CD: GitHub Actions build + release + AUR publish
- Shader library: cyberpunk, aurora, matrix, gradient, solid

### v1.1 — Features
- Terminal URL detection (Ctrl+click opens via url_handler)
- Terminal search (Ctrl+Shift+F)
- Calendar popup, workspace slide animation, maximize/restore animations
- Taskbar preview on hover
- Config-driven panel: workspaces, taskbar, clock, date, cpu, memory, battery, volume, network
- Panel as standalone module (top/bottom position, correct click handling)

### v1.2 — Functional DE
- Cross-window clipboard (server-side store)
- Focus-follows-mouse
- Logout/shutdown/reboot/suspend via systemd-logind D-Bus
- PipeWire volume control (panel widget + keybinds)
- Network status panel widget
- Notification click-to-dismiss
- URL handler (configurable, default w3m in terminal)
- Launcher frecency (launch history, score boosting)
- Screenshot to clipboard
- Session layout restore (save/load window positions)
- Proper DPMS (DRM connector property control)
- Accessibility: focus indicator, reduce animations, text size, large cursor
- Top panel tiling offset fix

### v2.0 — Graphical UI Toolkit (libvgp-gfx)
- Draw command protocol: 14 opcodes (rect, rounded_rect, circle, line, text, gradient, outline, clip)
- VGP_MSG_DRAW_COMMANDS: compact vector stream, 2-20KB/frame vs 1MB pixel surfaces
- VGP_MSG_THEME_INFO: 16 semantic colors + font metrics broadcast on connect + reload
- Server-side render_drawcmds() deserialization via NanoVG backend
- Client library: immediate-mode API (vgfx_rect, vgfx_text, vgfx_button, vgfx_slider, etc.)
- Widget library: button, checkbox, slider, dropdown, text_input, list, scrollbar, tooltip, progress
- All 6 apps migrated: settings, monitor, files, editor, bar, launcher
- Launcher removed PlutoVG dependency (pure draw commands now)

---

## Remaining

### Must Fix (bugs / incomplete implementations)
- [ ] NanoVG font metrics: char_advances currently hardcoded as monospace 0.6x — need real per-glyph measurement from nvgTextBounds()
- [ ] Outline rounded rect: render_drawcmds uses filled rect, need NanoVG nvgStroke() for true outlines
- [ ] Gradient rect: render_drawcmds uses banded approximation, need nvgLinearGradient() for smooth gradients
- [ ] Theme hot-reload broadcast: server reloads theme but doesn't re-send VGP_MSG_THEME_INFO to connected clients
- [ ] Clean up #if 0 dead code in vgp-settings/main.c
- [ ] Remove PlutoVG as hard dependency (make CPU backend build-optional or remove entirely)

### Should Do (quality / completeness)
- [ ] Monospace grid precision for terminal (zero ANSI rendering gaps)
- [ ] Blur behind transparent windows (FBO gaussian, GPU only)
- [ ] Theme packaging (.vgptheme zip archive with theme.toml + shaders)
- [ ] Monitor arrangement GUI in settings (visual drag & drop layout)
- [ ] Color picker widget for theme editing in settings
- [ ] Shader preview in background settings page
- [ ] Window rule editor in settings (add/remove/edit rules)
- [ ] Autostart editor in settings (add/remove programs)
- [ ] Keybind editor: full capture + save (currently display-only in new settings)
- [ ] Wiki update: document libvgp-gfx API, draw command protocol, theme system

### Nice to Have (future)
- [ ] Client SDK documentation + examples for third-party VGP apps
- [ ] Drag and drop between VGP applications
- [ ] Tabbed terminal (multiple shells in one vgp-term window)
- [ ] Built-in help overlay (Super+? shows keybind cheat sheet)
- [ ] Window snapping edge detection (magnetic edges)
- [ ] Per-monitor scaling (different text sizes per output)
- [ ] Plugin system for panel widgets (load .so modules)
- [ ] IPC control: expose draw command API for scripted UIs
