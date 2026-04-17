vgp-term(1)

# NAME

vgp-term - vector-rendered terminal emulator for VGP

# SYNOPSIS

*vgp-term* [*-e* _command_ [_args_...]]

# DESCRIPTION

Terminal emulator that renders every glyph as vector strokes using the VGP
stroke font. Runs inside a VGP client window.

# OPTIONS

*-e* _command_
	Run _command_ with _args_ in place of the user's login shell.

# FILES

_$XDG_CONFIG_HOME/vgp/terminal.toml_
	Font size, colours, scroll-back history length.

# SEE ALSO

*vgp*(1)
