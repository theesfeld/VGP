# Changelog

All notable changes to VGP are documented here.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.2.2] - 2026-04-17

### Added
- Full XDG Base Directory Specification compliance:
  `vgp_xdg_resolve`/`find_config`/`find_data` helpers in `libvgp`, covering
  config / data / state / cache / runtime.
- XDG Autostart Spec support: scans `$XDG_CONFIG_HOME/autostart` and each
  `$XDG_CONFIG_DIRS/autostart`, parses `.desktop` entries, respects
  `Hidden=true` and dedupes by filename.
- Modern FBO glass pipeline: scene FBO, 1/2 + 1/4 downsample chain, per-window
  glass fragment shader (Fresnel, IOR offset, chromatic aberration, top scatter).
  Toggleable via `VGP_FBO` env.
- Volumetric cumulus background shader with Henyey-Greenstein phase function,
  Beer-Lambert + powder, weather-map coverage mask, high-frequency erosion,
  ground layer below the horizon.
- `vgp-hud.h` shared HUD helpers: palette, etched/phosphor text,
  phosphor-glow line, target box, OSB buttons, full MFD frame, altitude tape.
- AppStream MetaInfo XML for every bundled app.
- Man pages (scdoc) for every binary plus `vgp.conf(5)`.
- `libvgp.pc` pkg-config file and installed public headers.
- `--version` / `-v` flag on the server binary.
- SPDX license identifiers in every source file.
- `CHANGELOG.md`, `CONTRIBUTING.md`, `SECURITY.md`, `CODE_OF_CONDUCT.md`,
  `.editorconfig`, GitHub Actions CI workflow.
- Shell completions (bash / zsh / fish) for the server.

### Changed
- All apps rewritten as F-16 MFD pages: SMS-launcher, DTE-files, DED-settings,
  SYMB-edit, engine-page monitor.
- Window chrome: photorealistic plexiglass, strong rounded corners (20 px),
  chromatic edge fringing, corner specular arc, focus halo.
- Palette rule: static decoration renders as black etching; dynamic values
  render as white / yellow / red phosphor with halation + bloom.
- `.desktop` files now pass `desktop-file-validate`: valid Categories,
  Keywords, StartupNotify, Icon keys added.
- Themes and shaders install to `$datadir/vgp/` and are discovered via
  `XDG_DATA_DIRS` rather than hardcoded `~/.config/vgp` paths.

### Fixed
- Window drag-between-monitors: window workspace now follows its centre,
  preventing it from disappearing mid-drag when crossing an output boundary.
- Starfield shader reduced from 200 per-pixel iterations to a tile-hash
  grid; eliminates GPU hang at ultrawide resolutions.

## [0.1.0] - 2026-04-17

Initial public release.
