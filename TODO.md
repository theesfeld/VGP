# VGP Development Roadmap

## Completed

### Phase 1-3: Core Server
- [x] DRM/KMS + GPU rendering (NanoVG/GLES3)
- [x] libinput + xkbcommon input
- [x] Window management (float, z-order, focus, move, resize, snap)
- [x] Multi-monitor + per-monitor workspaces
- [x] IPC (Unix socket, binary protocol, cell grid, pixel surfaces)
- [x] Configurable keybinds + user config (TOML)
- [x] Application launcher (fuzzy search .desktop files)
- [x] libseat (run without root)
- [x] Shader background system (GLSL, live-reload capable)

### Phase 4: Core DE Features
- [x] Taskbar + clickable workspace indicators + clock
- [x] Window snapping (Super+Arrow half/quarter tiling)
- [x] Workspace switching (Super+N) + move windows (Super+Shift+N)
- [x] D-Bus notification daemon
- [x] Clipboard (copy/paste between windows)
- [x] Screenshots (PrintScreen)
- [x] Panel click handling

### Phase 4.5: True Vector Terminal
- [x] Cell grid protocol (server-side NanoVG text rendering)
- [x] Terminal selection + copy/paste + bracketed paste
- [x] Scrollback (10k lines, mouse wheel, Shift+PageUp/Down)
- [x] Terminal config (~/.config/vgp/terminal.toml)

### Phase 5: Visual Polish
- [x] Window open/close animations (scale + opacity)
- [x] Drop shadows behind windows
- [x] Window opacity (active/inactive configurable)
- [x] Context-sensitive cursor shapes
- [x] Expose/overview mode (Super+Tab)

### Phase 6: Ecosystem Apps
- [x] libvgp-ui: shared cell grid UI toolkit
- [x] vgp-settings: GUI config editor (tabs, theme browser, panel config)
- [x] vgp-files: file manager (navigation, keyboard, icons)
- [x] vgp-monitor: system monitor (CPU/RAM graphs, progress bars)
- [x] vgp-view: image viewer (PNG/JPEG via stb_image)
- [x] .desktop files for all apps + greeter session file
- [x] Panel widget config (left/center/right widget placement)

---

## Remaining

### Phase 4 Leftovers
- [ ] Window rules -- per-app settings (float, workspace, size) in config
- [ ] Panel hover effects -- highlight entries on mouse over
- [ ] Middle-click taskbar to close window
- [ ] Right-click desktop context menu
- [ ] Panel widget rendering from config (currently hardcoded layout)

### Phase 4.5 Leftovers
- [ ] Monospace grid precision -- zero sub-pixel gaps in ANSI blocks
- [ ] Terminal font switching at runtime (Ctrl+Plus/Minus)
- [ ] Terminal URL detection + clickable URLs
- [ ] Terminal search (Ctrl+Shift+F)

### Phase 4.6: Theme Polish
- [ ] NERV theme fully styled (angular decorations, warning stripes)
- [ ] Light theme fully styled
- [ ] Dark/light mode toggle keybind (Super+Shift+T)
- [ ] Theme hot-reload (inotify on theme.toml)
- [ ] Background shader library (aurora, matrix rain, gradient, image wallpaper)
- [ ] Calendar popup widget (click clock)
- [ ] Shader browser in settings GUI
- [ ] Theme packaging format (.vgptheme zip)

### Phase 5 Leftovers
- [ ] Blur behind transparent windows (FBO gaussian pass)
- [ ] Smooth workspace slide animation
- [ ] Window preview on taskbar hover
- [ ] Notification animations (fade, slide, explode -- pluggable)
- [ ] Pluggable window animations (wobbly, flame, dissolve)

### Phase 6 Leftovers
- [ ] vgp-edit: text editor with syntax highlighting
- [ ] vgp-bar: standalone polybar-like bar with scripting
- [ ] Settings GUI: editable fields (not just display), write-back to config
- [ ] Settings GUI: background mode selector (solid/shader/wallpaper/none)
- [ ] Settings GUI: wallpaper file picker
- [ ] Settings GUI: shader effect previews
- [ ] VGP client SDK documentation

### Phase 7: Compatibility + Production
- [ ] XWayland bridge (run X11/Wayland apps inside VGP)
- [ ] Drag and drop between windows
- [ ] Multi-monitor arrangement GUI (drag to position)
- [ ] Session management (save/restore layout)
- [ ] Lock screen with shader background
- [ ] Power management (idle, DPMS, suspend)
- [ ] Auto-start programs from config
- [ ] IPC control protocol (external script interface, like swaymsg)
- [ ] Accessibility (keyboard nav, high contrast, screen reader)
- [ ] AUR package (PKGBUILD)
