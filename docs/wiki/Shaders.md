# Shader Development

VGP supports custom GLSL fragment shaders for backgrounds and UI elements.

## Shader Location

Place `.frag` files in:
- `~/.config/vgp/shaders/` (user shaders)
- `~/.config/vgp/themes/<name>/shaders/` (theme-bundled)

## Writing a Shader

Shaders implement a single function:

```glsl
void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    // uv: 0..1 within the element
    // pixel: absolute screen coordinates
    // color: output RGBA
}
```

## Available Uniforms

| Uniform | Type | Description |
|---------|------|-------------|
| `u_time` | float | Seconds since server start (for animation) |
| `u_resolution` | vec2 | Screen width, height |
| `u_rect` | vec4 | Element rectangle (x, y, w, h) |
| `u_color` | vec4 | Theme base color |
| `u_accent` | vec4 | Theme accent color |
| `u_mouse` | vec2 | Cursor position in pixels |
| `u_windows[8]` | vec4[] | Window rectangles (x, y, w, h) |
| `u_window_count` | int | Number of windows |

## Example: Animated Gradient

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

## Example: Mouse-Reactive Glow

```glsl
void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    float d = length(pixel - u_mouse) / u_resolution.x;
    float glow = exp(-d * 5.0) * 0.3;
    color = vec4(u_color.rgb + u_accent.rgb * glow, 1.0);
}
```

## Window Shadow Casting

Use `u_windows` and `u_window_count` to cast shadows from the mouse light source onto the background behind windows. See the default dark theme shader for a complete example.

## Hot Reload

Shaders are loaded at startup. Theme hot-reload (editing a shader and seeing changes live) is planned.

---

← **[Themes](Themes)** · Next: **[Architecture](Architecture)** →
