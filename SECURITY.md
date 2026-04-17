# Security Policy

## Reporting a vulnerability

Email a description of the issue to **tj.theesfeld@citywide.io**. Do not open
a public issue for security-sensitive reports.

Please include:

- VGP version (`vgp --version`) and commit hash (`git rev-parse HEAD`).
- Minimal reproducer if one exists.
- Impact you believe the issue has (code execution, information disclosure,
  denial-of-service, privilege escalation).

You will receive an acknowledgement within **72 hours**. Triage + a fix
timeline within **7 days**.

## Scope

In scope:

- Compositor process (`vgp`): input handling, IPC socket, protocol parsing,
  FBO / GL state, D-Bus notification endpoint.
- Client library (`libvgp`): protocol encode/decode, wire-format validation.
- Bundled apps: file-manager path handling, terminal escape-sequence parsing,
  editor file I/O.

Out of scope:

- Issues requiring local shell access prior to exploitation.
- Denial-of-service via the compositor's own config file (users own their
  config).
- Known issues in upstream deps (libdrm / libinput / plutovg / nanovg /
  libseat / libpam / dbus) — please report those upstream.

## Disclosure

Coordinated disclosure. Once a fix is merged and released, we publish a
GitHub Security Advisory referencing the commit and, if applicable, a CVE.
