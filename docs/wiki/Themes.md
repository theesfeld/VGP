# Themes

A theme is a named directory of TOML + optional GLSL that controls every
visual aspect of VGP.

## Where themes live

Resolved in this order (first hit wins):

1. `$XDG_CONFIG_HOME/vgp/themes/<name>/theme.toml`
2. `$XDG_CONFIG_DIRS/vgp/themes/<name>/theme.toml`
3. `$XDG_DATA_HOME/vgp/themes/<name>/theme.toml`
4. `$XDG_DATA_DIRS/vgp/themes/<name>/theme.toml`
   (typically `/usr/share/vgp/themes/...` for system themes)

## Switching themes

In `config.toml`:

```toml
[general]
theme = "hud"
```

Or open `vgp-settings` (**Super+S**) → **Theme** page and click one.
Changes take effect on the next SIGHUP to `vgp` (`pkill -HUP vgp`); the
settings editor sends the signal for you.

## Theme directory layout

```
~/.config/vgp/themes/my-theme/
├── theme.toml           # colours + geometry
└── shaders/             # optional GLSL fragment shaders
    ├── background.frag
    └── panel.frag
```

## Bundled default

The repository ships `themes/default.toml`. Distro packages install the
full `themes/` tree to `$datadir/vgp/themes/`, discoverable via the XDG
data-dirs fallback above. To override, copy the directory into
`$XDG_CONFIG_HOME/vgp/themes/` and edit.

## theme.toml — every key the parser reads

The parser ignores section headers; you can organise keys under any
section you like, or put them all at the top level. TOML comments (`#`)
are fine. Unknown keys are silently skipped.

```toml
# theme.toml — full key reference

# --- Geometry (pixels / points) ---
titlebar_height     = 32.0
border_width        = 1.0
corner_radius       = 20.0
statusbar_height    = 30.0

# --- Colors (hex RGB, leading # optional) ---
# Compositor-side alpha is applied by the glass pipeline; the hex value
# is interpreted as a fully-opaque colour.
titlebar_active     = "#A6C7F2"      # cool-blue glass tint
titlebar_inactive   = "#8CA6CD"
border_active       = "#FFD700"      # accent -- yellow
border_inactive     = "#666666"
background          = "#000000"      # fallback solid colour if no shader
statusbar_bg        = "#16213E"      # panel plexi tint
statusbar_text      = "#FFFFF2"
close_btn           = "#CCCCCC"
maximize_btn        = "#CCCCCC"
minimize_btn        = "#CCCCCC"
```

## Inline overrides in `config.toml`

Any theme key can be overridden globally from the main config without
cloning the theme directory:

```toml
[theme]
corner_radius = 24
border_active = "#FF8800"
```

See [Configuration → `[theme]`](Configuration#theme) for the context.

## Shaders referenced by themes

The compositor searches for background and panel shaders at:

1. `$XDG_CONFIG_HOME/vgp/shaders/background.frag` (or `panel.frag`)
2. `$XDG_CONFIG_DIRS/vgp/shaders/...`
3. `$XDG_DATA_HOME/vgp/shaders/...`
4. `$XDG_DATA_DIRS/vgp/shaders/...`

The bundled `themes/shaders/` directory is installed to
`/usr/share/vgp/shaders/` and picked up by the search path. See
[Shaders](Shaders) for how to write your own.

## Sharing a theme

```bash
tar czf my-theme.tar.gz -C ~/.config/vgp/themes my-theme
```

Recipients extract into `$XDG_CONFIG_HOME/vgp/themes/` and set
`[general] theme = "my-theme"`.

---

← **[Configuration](Configuration)** · Next: **[Shaders](Shaders)** →
