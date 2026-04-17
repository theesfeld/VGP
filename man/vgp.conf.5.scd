vgp.conf(5)

# NAME

vgp.conf - VGP configuration file

# DESCRIPTION

VGP reads its configuration from _$XDG_CONFIG_HOME/vgp/config.toml_ at
start-up. The file follows TOML syntax.

# SECTIONS

*[general]*
	General session options: _terminal_, _launcher_, _theme_, _workspaces_,
	_screenshot_dir_, _url_handler_, _font_size_, _focus_follows_mouse_,
	_wm_mode_ (floating / tiling / hybrid), _tile_algorithm_,
	_tile_master_ratio_, _tile_gap_inner_, _tile_gap_outer_,
	_tile_smart_gaps_.

*[input]*
	Pointer and keyboard: _pointer_speed_, _natural_scrolling_,
	_tap_to_click_, _repeat_delay_, _repeat_rate_.

*[keybinds]*
	A table of keyboard shortcut -> action pairs. Actions are either
	internal commands (e.g. _spawn_terminal_, _close_window_,
	_workspace_1_, _snap_left_) or shell spawns prefixed with *exec:*,
	e.g. _Super+s = "exec:vgp-settings"_.

*[monitor.*N*]*
	Per-output layout: _x_, _y_, _workspace_.

*[panel]*
	Panel (status-bar) geometry: _position_ (top / bottom), _height_.

*[panel.widgets.*slot*]*
	Widget lists for the _left_, _center_, and _right_ slots. Valid
	widgets: _workspaces_, _taskbar_, _clock_, _date_, _cpu_, _memory_,
	_battery_, _volume_, _network_, _settings_, _monitor_.

*[lockscreen]*
	_enabled_, _timeout_ (minutes).

*[session]*
	Autostart entries: each _autostart = "command"_ line is spawned at
	session start. XDG Autostart entries in
	_$XDG_CONFIG_HOME/autostart/_ are also run.

*[theme]*
	Override theme colours inline. Keys mirror the theme .toml format.

*[accessibility]*
	_high_contrast_, _focus_indicator_, _large_cursor_,
	_reduce_animations_, _text_size_.

# EXAMPLE

```
[general]
terminal = "vgp-term"
launcher = "vgp-launcher"
theme = "hud"
workspaces = 3
wm_mode = "floating"

[input]
pointer_speed = 3.9
tap_to_click = true

[keybinds]
Super+Return = "spawn_terminal"
Super+d = "spawn_launcher"
Super+q = "close_window"
Alt+Tab = "focus_next"
Print = "screenshot"

[panel]
position = "top"
height = 32
```

# SEE ALSO

*vgp*(1), *vgp-settings*(1)
