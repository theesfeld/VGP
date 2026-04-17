# Installation

Every tagged release on GitHub produces pre-built packages for Arch, Debian,
Ubuntu, Fedora, RHEL, and openSUSE, plus a source tarball. They're attached
to the [GitHub Releases page](https://github.com/theesfeld/VGP/releases).

## Pre-built packages

### Arch Linux / CachyOS / Manjaro

From the AUR:

```bash
yay -S vgp-git
# or
paru -S vgp-git
```

Or download the Arch tarball from the latest release and extract directly to
root (not recommended for long-term use — prefer the AUR package):

```bash
curl -LO https://github.com/theesfeld/VGP/releases/latest/download/vgp-0.1.0-arch-x86_64.tar.gz
sudo tar xzf vgp-0.1.0-arch-x86_64.tar.gz -C /
```

### Debian / Ubuntu

Download the three `.deb` files from the latest release and install together
so dependencies resolve in one pass:

```bash
V=0.1.0
BASE="https://github.com/theesfeld/VGP/releases/download/v${V}"
curl -LO "${BASE}/vgp_${V}_amd64.deb"
curl -LO "${BASE}/libvgp0_${V}_amd64.deb"
curl -LO "${BASE}/libvgp-dev_${V}_amd64.deb"   # optional: headers + pkg-config
sudo apt install ./vgp_${V}_amd64.deb ./libvgp0_${V}_amd64.deb
```

`libvgp-dev` is only needed if you are building third-party clients against
`libvgp`.

### Fedora / RHEL / CentOS

```bash
V=0.1.0
BASE="https://github.com/theesfeld/VGP/releases/download/v${V}"
sudo dnf install \
    "${BASE}/vgp-${V}-1.fc$(rpm -E %fedora).x86_64.rpm" \
    "${BASE}/vgp-libs-${V}-1.fc$(rpm -E %fedora).x86_64.rpm"
# optional: headers + pkg-config
sudo dnf install "${BASE}/vgp-devel-${V}-1.fc$(rpm -E %fedora).x86_64.rpm"
```

### openSUSE

```bash
sudo zypper install \
    https://github.com/theesfeld/VGP/releases/download/v0.1.0/vgp-0.1.0-1.x86_64.rpm \
    https://github.com/theesfeld/VGP/releases/download/v0.1.0/vgp-libs-0.1.0-1.x86_64.rpm
```

### Verify with SHA256SUMS

Every release ships a `SHA256SUMS` file. Always verify before installing:

```bash
curl -LO https://github.com/theesfeld/VGP/releases/download/v0.1.0/SHA256SUMS
sha256sum -c SHA256SUMS --ignore-missing
```

## Building from source

### Dependencies

| Purpose                | Runtime           | Dev                    |
| ---                    | ---               | ---                    |
| DRM/KMS                | `libdrm`          | `libdrm-dev`           |
| Input                  | `libinput`        | `libinput-dev`         |
| Keyboard layout        | `xkbcommon`       | `libxkbcommon-dev`     |
| Seat/session           | `libseat`         | `libseat-dev`          |
| D-Bus (notifications)  | `dbus`            | `libdbus-1-dev`        |
| Terminal emulation     | `libvterm`        | `libvterm-dev`         |
| GPU context            | `mesa` (GBM/EGL)  | `libgbm-dev`, `libegl-dev`, `libgles-dev` |
| Authentication         | `pam`             | `libpam0g-dev`         |
| Man pages (optional)   | —                 | `scdoc`                |

#### Per-distro install commands

Arch / CachyOS / Manjaro:

```bash
sudo pacman -S --needed meson ninja pkgconf \
    libdrm libinput libxkbcommon libseat dbus libvterm mesa pam scdoc
```

Debian / Ubuntu:

```bash
sudo apt install meson ninja-build pkgconf \
    libdrm-dev libinput-dev libudev-dev libxkbcommon-dev \
    libseat-dev libdbus-1-dev libvterm-dev \
    libgbm-dev libegl-dev libgles-dev libpam0g-dev scdoc
```

Fedora / RHEL:

```bash
sudo dnf install meson ninja-build pkgconf-pkg-config \
    libdrm-devel libinput-devel libudev-devel libxkbcommon-devel \
    libseat-devel dbus-devel libvterm-devel \
    mesa-libgbm-devel mesa-libEGL-devel mesa-libGLES-devel \
    pam-devel scdoc
```

openSUSE:

```bash
sudo zypper install meson ninja pkg-config \
    libdrm-devel libinput-devel libxkbcommon-devel \
    libseat-devel dbus-1-devel libvterm-devel \
    Mesa-libgbm-devel Mesa-libEGL-devel Mesa-libGLESv2-devel \
    pam-devel scdoc
```

Void Linux:

```bash
sudo xbps-install meson ninja pkg-config \
    libdrm-devel libinput-devel libxkbcommon-devel \
    libseat-devel dbus-devel libvterm-devel mesa-devel pam-devel scdoc
```

Gentoo:

```bash
sudo emerge meson ninja \
    dev-libs/libdrm dev-libs/libinput dev-libs/libxkbcommon \
    sys-auth/seatd sys-apps/dbus dev-libs/libvterm media-libs/mesa \
    sys-libs/pam app-text/scdoc
```

NixOS flake shell:

```nix
devShells.default = pkgs.mkShell {
  nativeBuildInputs = with pkgs; [ meson ninja pkg-config scdoc ];
  buildInputs = with pkgs; [
    libdrm libinput libxkbcommon seatd dbus libvterm mesa pam
  ];
};
```

### Build + install

```bash
git clone https://github.com/theesfeld/VGP.git
cd VGP
meson setup build --prefix=/usr --buildtype=release
meson compile -C build
sudo meson install -C build
```

This installs:

| Path                                                    | Contents                            |
| ---                                                     | ---                                 |
| `/usr/bin/vgp`, `/usr/bin/vgp-*`                        | Compositor + bundled apps           |
| `/usr/lib/libvgp.so.*`                                  | Versioned client library            |
| `/usr/include/vgp/`                                     | Public headers                      |
| `/usr/lib/pkgconfig/libvgp.pc`                          | pkg-config file                     |
| `/usr/share/wayland-sessions/vgp.desktop`               | Session entry for display managers  |
| `/usr/share/applications/vgp-*.desktop`                 | Application launchers               |
| `/usr/share/metainfo/io.github.theesfeld.vgp*.xml`      | AppStream metadata                  |
| `/usr/share/icons/hicolor/scalable/apps/vgp*.svg`       | SVG icons                           |
| `/usr/share/man/man1/vgp*.1`, `man5/vgp.conf.5`         | Man pages (if scdoc was installed)  |
| `/usr/share/vgp/themes/`, `/usr/share/vgp/data/`        | Bundled themes + shaders            |
| `/usr/share/bash-completion/completions/vgp` etc.       | Shell completions                   |

### Running

From a display manager (GDM / SDDM / LightDM / Ly / greetd): select **VGP**
as the session and log in.

From a TTY (Ctrl+Alt+F2):

```bash
vgp
```

Use `vgp --help` for options, `vgp --version` for the running version.

## First-run checklist

1. Launch: press **Super+Return** for a terminal, **Super+D** for the
   launcher, **Super+S** for settings.
2. Edit `$XDG_CONFIG_HOME/vgp/config.toml` (see [Configuration](Configuration)).
3. Switch theme in `[general] theme = "hud"` (see [Themes](Themes)).
4. Logs live at `$XDG_STATE_HOME/vgp/vgp.log` if anything misbehaves.

---

Next: **[Configuration](Configuration)** →
