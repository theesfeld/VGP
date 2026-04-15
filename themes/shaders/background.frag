/* VGP Background: Animated aurora / northern lights */

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * vnoise(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec2 p = pixel / u_resolution;
    float t = u_time * 0.15;

    /* Deep dark base */
    vec3 dark = u_color.rgb * 0.3;

    /* Aurora bands */
    float n1 = fbm(vec2(p.x * 3.0 + t, p.y * 0.5 + t * 0.3));
    float n2 = fbm(vec2(p.x * 2.0 - t * 0.7, p.y * 0.8 + t * 0.2));

    /* Vertical falloff -- aurora is stronger at the top */
    float falloff = smoothstep(1.0, 0.2, p.y);

    vec3 accent = u_accent.rgb;
    vec3 green = vec3(0.1, 0.8, 0.4);
    vec3 purple = vec3(0.5, 0.1, 0.8);

    vec3 aurora = mix(green, accent, n1) * n2 * falloff * 0.4;
    aurora += purple * n1 * falloff * 0.15;

    /* Stars */
    float star = step(0.998, hash(floor(pixel * 0.5)));
    float twinkle = sin(u_time * 3.0 + hash(floor(pixel * 0.5)) * 100.0) * 0.5 + 0.5;
    vec3 stars = vec3(star * twinkle * 0.6);

    color = vec4(dark + aurora + stars, 1.0);
}
