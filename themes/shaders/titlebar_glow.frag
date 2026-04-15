/* VGP theme shader: glowing titlebar with subtle pulse */
void theme_main(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec3 base = u_focused > 0.5 ? u_colors[2].rgb : u_colors[3].rgb;
    vec3 accent = u_colors[1].rgb;

    /* Subtle horizontal gradient */
    float grad = smoothstep(0.0, 1.0, uv.x);

    /* Pulse on focused windows */
    float pulse = u_focused * sin(u_time * 2.0) * 0.03 + 1.0;

    vec3 c = mix(base, accent, grad * 0.15) * pulse;
    color = vec4(c, 1.0);
}
