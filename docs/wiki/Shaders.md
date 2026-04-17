# Shaders

VGP compiles user-supplied GLSL fragment shaders and runs them as the
desktop background and the panel overlay. Shaders receive uniforms for
time, resolution, cursor position, and the list of on-screen windows,
and draw into a full-screen quad.

## Where shaders live

Resolved in this order (first hit wins):

1. `$XDG_CONFIG_HOME/vgp/shaders/<name>.frag`
2. `$XDG_CONFIG_DIRS/vgp/shaders/<name>.frag` (default `/etc/xdg/vgp/...`)
3. `$XDG_DATA_HOME/vgp/shaders/<name>.frag`
4. `$XDG_DATA_DIRS/vgp/shaders/<name>.frag`
   (typically `/usr/share/vgp/shaders/...` for distro-installed shaders)

Two well-known names are loaded automatically at compositor start:

| Name              | Role                                     |
| ---               | ---                                      |
| `background.frag` | Desktop background, covers every output  |
| `panel.frag`      | Optional fill for the status-bar strip   |

Shaders are **not** currently loaded from theme subdirectories; the
compositor only reads from the XDG shader paths above. Ship a
theme-specific shader by copying it into
`$XDG_CONFIG_HOME/vgp/shaders/` or installing it to
`$datadir/vgp/shaders/`.

## Bundled shaders

The repository ships these under `themes/shaders/`; distro packages
install them to `/usr/share/vgp/shaders/`:

| File                 | What it does                                              |
| ---                  | ---                                                       |
| `clouds.frag`        | Volumetric cumulus + sky + ground. Henyey-Greenstein phase, Beer–Lambert + powder, high-frequency erosion, 20-minute sun drift. The current default `background.frag`. |
| `starfield.frag`     | Cheap tile-hashed starfield. Swap it in by copying it over `background.frag`. |
| `titlebar_glow.frag` | Legacy titlebar glow effect.                              |

## Shader authoring

Every VGP shader implements a single entry point. The loader prepends a
preamble with the standard uniforms (see
`src/server/shader_loader.c`) and appends a `main()` that calls
`effect()`:

```glsl
void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    // uv: 0..1 within the element
    // pixel: absolute screen coordinates in pixels
    // color: RGBA output (write to this)
}
```

## Uniforms exposed to every shader

| Uniform          | Type       | Value                                       |
| ---              | ---        | ---                                         |
| `u_time`         | `float`    | Seconds since compositor start, monotonic   |
| `u_resolution`   | `vec2`     | Output width, height in pixels              |
| `u_rect`         | `vec4`     | Element rect `(x, y, w, h)` in pixels       |
| `u_color`        | `vec4`     | `theme.background` as RGBA                  |
| `u_accent`       | `vec4`     | `theme.border_active` as RGBA               |
| `u_mouse`        | `vec2`     | Cursor position in pixels, on this output   |
| `u_windows`      | `vec4[8]`  | Visible window rects `(x, y, w, h)`         |
| `u_window_count` | `int`      | Number of populated entries in `u_windows`  |

Precision is `highp float` by default; GLSL version is `300 es`.

## Example — animated gradient

```glsl
void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec2 p = pixel / u_resolution;
    float t = u_time * 0.2;
    vec3 c1 = u_color.rgb;
    vec3 c2 = u_accent.rgb;
    float grad = smoothstep(0.0, 1.0, p.y + sin(p.x * 3.0 + t) * 0.1);
    color = vec4(mix(c1, c2, grad * 0.3), 1.0);
}
```

## Example — mouse-reactive glow

```glsl
void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    float d = length(pixel - u_mouse) / u_resolution.x;
    float glow = exp(-d * 5.0) * 0.3;
    color = vec4(u_color.rgb + u_accent.rgb * glow, 1.0);
}
```

## Example — window shadow casting

`u_windows` + `u_window_count` let you treat each visible window as an
occluder between a light source (typically `u_mouse`) and the shaded
pixel. See `themes/shaders/clouds.frag` for a volumetric example; for a
simple 2D shadow:

```glsl
bool occluded(vec2 p) {
    for (int i = 0; i < u_window_count; i++) {
        vec4 w = u_windows[i];
        if (p.x > w.x && p.x < w.x + w.z &&
            p.y > w.y && p.y < w.y + w.w) return true;
    }
    return false;
}
```

## Hot reload

VGP watches `$XDG_CONFIG_HOME/vgp/shaders/` with inotify (via
`src/server/hotreload.c`). Saving a shader file triggers a re-render,
and editing the config triggers a full theme broadcast to every
connected client. Re-compiling the shader program from the edited file
is not yet wired up — the inotify hook currently schedules a frame but
keeps the previously compiled program. Send `SIGHUP` to `vgp`
(or restart it) to pick up new GLSL.

## Performance budget

The background shader runs once per pixel per frame across every
output. Heavy per-pixel loops become a problem fast at ultrawide
resolutions. Rough guidelines, measured on Intel UHD-class graphics at
3440×1440 @ 60 Hz:

- Keep per-pixel noise / fBM evaluations under ~300 taps worst case.
- Fixed-size `for (int i = 0; i < N; i++)` loops optimise better than
  dynamic-bound loops on most drivers.
- Avoid 3D noise unless you really need it — 2D + height shaping is
  cheaper and usually enough.
- The bundled `clouds.frag` sits at the ceiling: 18-step raymarch +
  4-step light march + 3-octave 2D fBM stays within budget.

---

← **[Themes](Themes)** · Next: **[Architecture](Architecture)** →
