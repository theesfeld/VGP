vgp(1)

# NAME

vgp - GPU-accelerated vector display server

# SYNOPSIS

*vgp* [_options_]

# DESCRIPTION

*vgp* is a DRM/KMS-based vector display server. It renders all chrome, text
and widgets as vectors on GLES3 via nanovg, with translucent glass windows
composited by an FBO pipeline. No X11 or Wayland is required.

The server handles input (libinput), keyboard/mouse seat (libseat), display
mode setting (libdrm), GPU context (GBM/EGL), and hosts client applications
over a Unix-domain protocol socket.

# OPTIONS

*--config* _PATH_
	Use _PATH_ as the config file instead of the default
	_$XDG_CONFIG_HOME/vgp/config.toml_.

*--version*, *-v*
	Print the program version and exit.

*--help*, *-h*
	Show a brief usage summary.

# FILES

_$XDG_CONFIG_HOME/vgp/config.toml_
	Main configuration (see *vgp.conf*(5)).

_$XDG_CONFIG_HOME/vgp/themes/_<name>_/theme.toml_
	Per-theme overrides.

_$XDG_STATE_HOME/vgp/vgp.log_
	Server log.

_$XDG_STATE_HOME/vgp/session.json_
	Last saved window layout.

_$XDG_RUNTIME_DIR/vgp-0_
	Client protocol socket.

_$XDG_RUNTIME_DIR/vgp-ctl_
	Control IPC socket.

# ENVIRONMENT

*VGP_CPU*
	Set to _1_ to force the CPU fallback renderer (plutovg).

*VGP_FBO*
	Set to _0_ to disable the FBO glass compositing pipeline.

*XDG_CONFIG_HOME*, *XDG_STATE_HOME*, *XDG_DATA_HOME*, *XDG_DATA_DIRS*,
*XDG_CONFIG_DIRS*, *XDG_RUNTIME_DIR*
	Honoured per the XDG Base Directory Specification.

# SEE ALSO

*vgp-term*(1), *vgp-launcher*(1), *vgp-files*(1), *vgp-edit*(1),
*vgp-monitor*(1), *vgp-settings*(1), *vgp-bar*(1)

# AUTHORS

William Theesfeld.
