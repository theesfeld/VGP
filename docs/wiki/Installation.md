# Installation

## Arch Linux / CachyOS (AUR)

The recommended way to install on Arch-based distributions:

```bash
yay -S vgp-git
```

Or manually:
```bash
git clone https://aur.archlinux.org/vgp-git.git
cd vgp-git
makepkg -si
```

## Building from Source

### Dependencies

| Dependency | Description |
|-----------|-------------|
| meson | Build system |
| ninja | Build backend |
| libdrm | DRM/KMS kernel interface |
| libinput | Input device handling |
| xkbcommon | Keyboard layout handling |
| libseat | Session/seat management (logind) |
| dbus | D-Bus IPC (notifications) |
| libvterm | Terminal emulation |
| mesa | OpenGL ES (GPU rendering) |
| libcrypt | Password verification (lock screen) |

### Package names by distribution

**Arch / CachyOS / Manjaro:**
```bash
pacman -S meson ninja libdrm libinput libxkbcommon seatd dbus libvterm mesa
```

**Fedora:**
```bash
dnf install meson ninja-build libdrm-devel libinput-devel libxkbcommon-devel \
    seatd-devel dbus-devel libvterm-devel mesa-libGLES-devel mesa-libgbm-devel \
    mesa-libEGL-devel libcrypt-devel
```

**Debian / Ubuntu:**
```bash
apt install meson ninja-build libdrm-dev libinput-dev libxkbcommon-dev \
    libseat-dev libdbus-1-dev libvterm-dev libgles2-mesa-dev libgbm-dev \
    libegl1-mesa-dev
```

**Void Linux:**
```bash
xbps-install meson ninja libdrm-devel libinput-devel libxkbcommon-devel \
    seatd-devel dbus-devel libvterm-devel mesa-devel
```

**NixOS:**
Add to your `environment.systemPackages` or use a shell:
```nix
nativeBuildInputs = [ meson ninja pkg-config ];
buildInputs = [ libdrm libinput libxkbcommon seatd dbus libvterm mesa ];
```

**Gentoo:**
```bash
emerge dev-libs/libdrm dev-libs/libinput dev-libs/libxkbcommon \
    sys-auth/seatd sys-apps/dbus dev-libs/libvterm media-libs/mesa
```

### Build

```bash
git clone https://github.com/theesfeld/VGP.git
cd VGP
meson setup build
meson compile -C build
```

### Install

```bash
sudo meson install -C build
```

This installs:
- `/usr/bin/vgp` -- display server
- `/usr/bin/vgp-term` -- terminal emulator
- `/usr/bin/vgp-launcher` -- application launcher
- `/usr/bin/vgp-settings` -- settings editor
- `/usr/bin/vgp-files` -- file manager
- `/usr/bin/vgp-monitor` -- system monitor
- `/usr/bin/vgp-view` -- image viewer
- `/usr/share/wayland-sessions/vgp.desktop` -- greeter session
- `/usr/share/applications/vgp-*.desktop` -- app entries

### Running

From your display manager / greeter, select "VGP" as the session.

Or from a TTY:
```bash
vgp --config ~/.config/vgp/config.toml
```

## First Run

On first run, VGP creates default config at `~/.config/vgp/config.toml`. You can customize keybinds, themes, monitor layout, and more. Press **Super+S** to open the settings editor.

### Default Keybinds

| Key | Action |
|-----|--------|
| Super+Return | Terminal |
| Super+D | Launcher |
| Super+Q | Close window |
| Super+Tab | Expose overview |
| Super+Space | Toggle float/tile |
| Super+L | Lock screen |
| Ctrl+Alt+Backspace | Quit |
