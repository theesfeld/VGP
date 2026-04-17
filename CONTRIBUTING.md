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

The release pipeline is triggered two ways — pick whichever is more
convenient. The end result is the same: a single GitHub Release with
`.deb` / `.rpm` / source / Arch artifacts attached and a release body
containing user notes + the auto-generated PR summary + our git-log
commit list + install instructions.

### A. GitHub UI ("Draft a new release") — recommended

1. Open **Releases → Draft a new release**.
2. **Choose a tag**: type the new tag name (e.g. `v0.2.0`) and select
   *Create new tag on publish*.
3. **Target**: pick `master` (or the exact commit you want to ship).
4. **Title**: `v0.2.0` (or any short title).
5. Click **Generate release notes** — GitHub fills the body with a
   PR-based "What's Changed" summary, author attribution, and a
   "New Contributors" block. Labels on those PRs drive the section
   grouping defined in `.github/release.yml`.
6. Edit the notes if you want; add Highlights / breaking-change
   callouts etc.
7. Click **Publish release**.

The moment the release is published, `.github/workflows/release.yml`
fires on the `release: published` event. It:

- Builds the source tarball, Arch prefix tree, Debian `.deb` trio, and
  Fedora `.rpm` trio — ~10 minutes in parallel.
- Uploads every artifact plus `SHA256SUMS` to the release.
- Appends a **Commits** section (full `git log` since the previous
  tag, one line per commit, with `@author` attribution — catches any
  direct push-to-master that the PR-based summary would miss) and an
  **Install** block to the release body.

Your hand-written notes and the UI auto-summary are preserved.

### B. Command line — `git push origin v0.2.0`

Useful for scripting / headless workflows.

```bash
# 1. (optional) Move everything under [Unreleased] into a new
#    ## [0.2.0] section in CHANGELOG.md.
vim CHANGELOG.md

# 2. Bump meson version.
sed -i "s/^  version : '.*'/  version : '0.2.0',/" meson.build

# 3. Commit + annotated tag + push the tag.
git commit -am "Release 0.2.0"
git tag -a v0.2.0 -m "Release 0.2.0"
git push origin master v0.2.0
```

On `push: tags` the workflow runs the same build matrix and creates
the Release from scratch — `generate_release_notes: true` produces
the PR summary, then our commit-log + install footer is appended.

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
