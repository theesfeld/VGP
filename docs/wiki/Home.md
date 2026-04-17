# VGP Wiki

GPU-accelerated vector display server and desktop environment for Linux.
Runs directly on DRM/KMS via GBM and EGL — no X11, no Wayland.

## What VGP is

- Every window, glyph, cursor, and decoration is vector-rendered on the GPU
  via NanoVG + GLES3.
- Photorealistic plexiglass window chrome composited through an FBO pipeline
  (scene + downsample chain + per-window glass fragment shader).
- Volumetric cumulus background with Henyey-Greenstein scattering.
- F-16 HUD/MFD UX across every bundled app.
- Fully configurable via TOML + a built-in settings editor.
- Standards-compliant: [XDG Base Directory][xdg-base],
  [XDG Autostart][xdg-auto], [Desktop Entry][desktop-entry],
  [AppStream][appstream], [Desktop Notifications][notif-spec],
  [SPDX][spdx], [Semantic Versioning][semver].

## What VGP is not

- Not an X11 / Wayland replacement for existing GUI apps. Firefox / Chrome /
  VS Code / Electron will not run natively.
- Not a Wayland compositor — VGP defines its own protocol (`libvgp`).
- Not production-stable. Early-stage, breaking changes expected.

## Start here

1. **[Installation](Installation)** — pre-built packages (Arch / Debian /
   Ubuntu / Fedora / RHEL / openSUSE) + build-from-source.
2. **[Configuration](Configuration)** — keybinds, monitors, panel, input,
   autostart.
3. **[Themes](Themes)** — color palette + geometry, hot-reload.
4. **[Shaders](Shaders)** — writing GLSL fragment shaders for backgrounds
   and panels.
5. **[Architecture](Architecture)** — internals, IPC protocol, render
   pipeline.
6. **[FAQ](FAQ)** — common questions.

## Project links

- [GitHub repository](https://github.com/theesfeld/VGP)
- [Issue tracker](https://github.com/theesfeld/VGP/issues)
- [Releases](https://github.com/theesfeld/VGP/releases) — `.deb`, `.rpm`,
  Arch tarball, source.
- [AUR package](https://aur.archlinux.org/packages/vgp-git)
- [CHANGELOG](https://github.com/theesfeld/VGP/blob/master/CHANGELOG.md)
- [Contributing](https://github.com/theesfeld/VGP/blob/master/CONTRIBUTING.md)
- [Security policy](https://github.com/theesfeld/VGP/blob/master/SECURITY.md)

[xdg-base]:     https://specifications.freedesktop.org/basedir-spec/latest/
[xdg-auto]:     https://specifications.freedesktop.org/autostart-spec/latest/
[desktop-entry]: https://specifications.freedesktop.org/desktop-entry-spec/latest/
[appstream]:    https://www.freedesktop.org/software/appstream/docs/
[notif-spec]:   https://specifications.freedesktop.org/notification-spec/latest/
[spdx]:         https://spdx.org/licenses/
[semver]:       https://semver.org/spec/v2.0.0.html
