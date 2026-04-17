# Contributing to VGP

Thanks for your interest. Short version: patches welcome, open an issue first
if the change is large, keep commits atomic.

## Ground rules

- **License**: VGP is MIT. Every source file carries
  `/* SPDX-License-Identifier: MIT */` on the first line. Match that in new
  files.
- **Style**: four-space indentation in C, two in meson, no tabs. An
  `.editorconfig` is present; if your editor supports it you'll get this for
  free. Run `clang-format` if you have one configured.
- **Warnings**: build is `-Wall -Wextra` via `warning_level=2`. A clean build
  is a merge requirement. Treat warnings as bugs.
- **Scope**: fix one thing per commit / PR. Refactors and feature work go in
  separate changes.

## Build

```
meson setup build
meson compile -C build
```

Optional deps: `scdoc` enables man-page generation. Without it, meson prints a
notice and skips the man-page targets.

To run the compositor from a TTY:

```
./build/vgp
```

Set `VGP_FBO=0` to disable the FBO glass pipeline (falls back to line-based
approximation). `VGP_CPU=1` forces the plutovg CPU renderer.

## Filing an issue

- For compositor/render bugs, include `$XDG_STATE_HOME/vgp/vgp.log`.
- Note your GPU, driver (`glxinfo -B` / `eglinfo`), and monitor layout.
- Reproduce steps; a screenshot helps if visual.

## Pull requests

- Target `master`.
- Include a one-line summary plus a short "why" in the commit body.
- Update `CHANGELOG.md` (Unreleased section) for user-visible changes.
- If you add a new public API, add the header under `include/vgp/` and list
  it in `install_headers(...)` in `meson.build`.
- If you touch paths, respect the XDG Base Directory Spec: use
  `vgp_xdg_resolve()` / `vgp_xdg_find_config()` / `vgp_xdg_find_data()`
  instead of hardcoding `$HOME/.config`.

## Standards

This codebase aims to be compliant with:

- [XDG Base Directory Specification](https://specifications.freedesktop.org/basedir-spec/latest/)
- [XDG Autostart Specification](https://specifications.freedesktop.org/autostart-spec/latest/)
- [Desktop Entry Specification](https://specifications.freedesktop.org/desktop-entry-spec/latest/)
- [AppStream Specification](https://www.freedesktop.org/software/appstream/docs/)
- [Desktop Notifications Specification](https://specifications.freedesktop.org/notification-spec/latest/)
- [SPDX License Identifiers](https://spdx.org/licenses/)
- [Semantic Versioning 2.0](https://semver.org/spec/v2.0.0.html)
- [Keep a Changelog 1.1](https://keepachangelog.com/en/1.1.0/)

If you change anything user-visible touching any of the above, double-check
the spec before merging.
