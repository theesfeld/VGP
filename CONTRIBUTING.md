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

## Releases

Release notes on the GitHub Releases page are assembled automatically
when a `v<version>` tag is pushed:

1. **PR-based summary** (top): GitHub's native `generate_release_notes`
   walks every merged PR since the previous tag and groups them by the
   label categories configured in `.github/release.yml` (Features, Bug
   fixes, Documentation, Packaging, UI / rendering, Other). Each entry
   reads `- PR title by @author in #N`.
2. **Highlights** (middle): the section of `CHANGELOG.md` whose heading
   matches the released version (`## [x.y.z]`). Maintain this by
   moving items out of `## [Unreleased]` into a new versioned section
   before tagging.
3. **Commits** (middle): a full `git log` since the previous tag, one
   line per commit, with a short SHA linked to the commit page and
   `@author` attribution. Catches direct pushes to `master` that the
   PR-based generator would miss.
4. **Full diff link**, **Artifacts** list, and **Install** table
   (bottom).

To cut a release:

```bash
# 1. Move everything under [Unreleased] into a new [0.2.0] section.
vim CHANGELOG.md

# 2. Bump meson version.
sed -i "s/^  version : '.*'/  version : '0.2.0',/" meson.build

# 3. Commit + tag + push.
git commit -am "Release 0.2.0"
git tag -a v0.2.0 -m "Release 0.2.0"
git push origin master v0.2.0
```

The release workflow (`.github/workflows/release.yml`) fans out into
source / arch / deb / rpm build jobs, collects everything into one
release, and attaches the assembled notes + `SHA256SUMS` to the GitHub
Release.

### PR labels that drive categorization

| Label                            | Section              |
| ---                              | ---                  |
| `breaking-change`, `breaking`    | 💥 Breaking changes  |
| `enhancement`, `feature`         | ✨ Features          |
| `bug`, `fix`                     | 🐛 Bug fixes         |
| `security`                       | 🔐 Security          |
| `documentation`, `docs`          | 📚 Documentation     |
| `packaging`, `build`             | 📦 Packaging         |
| `ui`, `render`, `shader`         | 🎨 UI / rendering    |
| (anything else)                  | Other changes        |

PRs authored by `dependabot` or `github-actions` are excluded.

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
