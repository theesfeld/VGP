# VGP

[![Build](https://github.com/theesfeld/VGP/actions/workflows/build.yml/badge.svg)](https://github.com/theesfeld/VGP/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/theesfeld/VGP?include_prereleases&sort=semver)](https://github.com/theesfeld/VGP/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![AUR](https://img.shields.io/aur/version/vgp-git?label=AUR%3A%20vgp-git)](https://aur.archlinux.org/packages/vgp-git)
[![Platform](https://img.shields.io/badge/platform-Linux%20DRM%2FKMS-black)](#)
[![C17](https://img.shields.io/badge/C-17-A8B9CC?logo=c)](#)

**GPU-accelerated vector display server and desktop environment for Linux.**
Runs directly on DRM/KMS via GBM and EGL — no X11, no Wayland.

- Every window, glyph, cursor, and decoration is vector-rendered on the GPU
  (NanoVG + GLES3).
- Photorealistic plexiglass window chrome composited through an FBO pipeline
  (scene + downsample chain + per-window glass fragment shader with Fresnel,
  IOR offset, chromatic aberration).
- Volumetric cumulus background (Henyey-Greenstein scattering, Beer–Lambert
  + powder, weather-map coverage, high-frequency erosion).
- F-16 HUD / MFD UX across every bundled app: `vgp-term`, `vgp-edit`,
  `vgp-files`, `vgp-launcher`, `vgp-monitor`, `vgp-settings`, `vgp-bar`,
  `vgp-view`.
- Fully [standards-compliant](CONTRIBUTING.md#standards): XDG Base Directory,
  XDG Autostart, Desktop Entry, AppStream, Desktop Notifications, SPDX,
  semver.

## Install

Pre-built packages attached to every [release][releases]:

| Distro                   | Command                                             |
| ---                      | ---                                                 |
| Arch / CachyOS / Manjaro | `yay -S vgp-git`                                    |
| Debian / Ubuntu          | `sudo apt install ./vgp_*.deb ./libvgp0_*.deb`      |
| Fedora / RHEL / CentOS   | `sudo dnf install ./vgp-*.rpm ./vgp-libs-*.rpm`     |
| openSUSE                 | `sudo zypper install ./vgp-*.rpm ./vgp-libs-*.rpm`  |
| Source                   | `meson setup build && sudo meson install -C build`  |

Full per-distro instructions, dependency lists, and `SHA256SUMS`
verification in **[wiki → Installation][wiki-install]**.

## Documentation

- **[Home][wiki-home]** — overview, what VGP is and isn't.
- **[Installation][wiki-install]** — per-distro packages + build from source.
- **[Configuration][wiki-config]** — keybinds, monitors, panel, input.
- **[Themes][wiki-themes]** — palette, geometry, hot-reload.
- **[Shaders][wiki-shaders]** — writing GLSL background / panel effects.
- **[Architecture][wiki-arch]** — render pipeline, IPC protocol, file layout.
- **[FAQ][wiki-faq]**

Reference:

- `man 1 vgp`, `man 5 vgp.conf` (installed with the package)
- [`CHANGELOG.md`](CHANGELOG.md)
- [`CONTRIBUTING.md`](CONTRIBUTING.md)
- [`SECURITY.md`](SECURITY.md)
- [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md)

## Support

- Bugs / feature requests → [GitHub Issues](https://github.com/theesfeld/VGP/issues)
- Security disclosures → [`SECURITY.md`](SECURITY.md)

## License

MIT. Every source file carries an SPDX identifier; full text in
[`LICENSE`](LICENSE).

[releases]:     https://github.com/theesfeld/VGP/releases
[wiki-home]:    https://github.com/theesfeld/VGP/wiki/Home
[wiki-install]: https://github.com/theesfeld/VGP/wiki/Installation
[wiki-config]:  https://github.com/theesfeld/VGP/wiki/Configuration
[wiki-themes]:  https://github.com/theesfeld/VGP/wiki/Themes
[wiki-shaders]: https://github.com/theesfeld/VGP/wiki/Shaders
[wiki-arch]:    https://github.com/theesfeld/VGP/wiki/Architecture
[wiki-faq]:     https://github.com/theesfeld/VGP/wiki/FAQ
