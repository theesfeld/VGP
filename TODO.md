# VGP Development Roadmap

## Done

Everything through v1.0:
- GPU vector rendering (NanoVG/GLES3), multi-monitor, tiling + floating
- Terminal (cell grid protocol, scrollback, selection, clipboard, font resize)
- Apps: launcher, settings (editable), files, monitor, editor, image viewer
- Config: themes, shaders, keybinds, panel widgets, window rules, tiling
- Lock screen (PAM), session management, power management, IPC control
- Context menus (desktop + titlebar), hot-reload (inotify)
- CI/CD: GitHub Actions build + release + AUR publish
- Wiki: 7 pages, comprehensive documentation
- Shader library: cyberpunk, aurora, matrix, gradient, solid

v1.1:
- Terminal URL detection + clickable links
- Terminal search (Ctrl+Shift+F)
- Calendar popup (click clock)
- NERV theme fully styled
- Light theme fully styled
- Workspace slide animation
- Maximize/restore animations
- Taskbar preview on hover (miniature window content)
- Config-driven panel widget rendering (workspaces, taskbar, clock, date, cpu, memory, battery)
- Panel top/bottom position from config
- vgp-bar: standalone scriptable status bar

v1.2:
- Session layout restore (save/load window positions, title-based matching)
- Proper DPMS power management (DRM connector property control)
- Accessibility: high contrast, focus indicator, font scale, reduce animations, large cursor
- Accessibility settings tab in vgp-settings GUI
- Pluggable animation system (maximize, restore, workspace slide)

## Remaining

### v1.3 (Polish)
- [ ] Blur behind transparent windows (FBO gaussian, GPU only)
- [ ] Monospace grid precision (zero ANSI gaps)
- [ ] Theme packaging (.vgptheme zip)

### v2.0+ (Future)
- [ ] Settings: color picker widget, shader previews
- [ ] Monitor arrangement GUI (drag & drop in settings)
- [ ] Drag and drop between applications
- [ ] Client SDK documentation + examples
