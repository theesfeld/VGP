vgp-launcher(1)

# NAME

vgp-launcher - SMS-style application launcher

# SYNOPSIS

*vgp-launcher*

# DESCRIPTION

Fuzzy-search application launcher rendered as an F-16 SMS (Stores Management
System) page. Scans _$XDG_DATA_HOME/applications_ and each
_$XDG_DATA_DIRS/applications_ for .desktop entries.

# FILES

_$XDG_STATE_HOME/vgp/launcher_history_
	Launch frequency history, used to weight result ranking.

# KEYBINDINGS

*Printable keys*
	Type to filter.

*Arrow up/down*
	Move selection.

*Enter*
	Launch selected app.

*Escape*
	Close the launcher.

# SEE ALSO

*vgp*(1)
