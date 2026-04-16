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

## Remaining

### v1.1
- [ ] Blur behind transparent windows (FBO gaussian)
- [ ] Smooth workspace slide animation
- [ ] Taskbar window preview on hover
- [ ] Terminal URL detection + clickable links
- [ ] Terminal search (Ctrl+Shift+F)
- [ ] Calendar popup (click clock)
- [ ] Monospace grid precision (zero ANSI gaps)
- [ ] NERV theme fully styled (angular decorations)
- [ ] Light theme fully styled
- [ ] Theme packaging (.vgptheme zip)
- [ ] Pluggable animation system
- [ ] Panel widget rendering from config

### v1.2+
- [ ] vgp-bar: standalone scriptable bar
- [ ] Settings: color picker, shader previews
- [ ] Monitor arrangement GUI
- [ ] Drag and drop
- [ ] Session layout restore
- [ ] Accessibility
- [ ] Client SDK docs
- [ ] DPMS proper implementation
