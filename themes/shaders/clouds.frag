/* VGP Background: blue sky + volumetric clouds, flying forward.
 * No raymarch loop -- two fBM layers composited over a sky gradient.
 * Designed for 3440x1440+ at 60fps. Stays cheap: 8 noise taps per pixel. */

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

/* 4-octave fBM -- fixed iteration count */
float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    for (int i = 0; i < 4; i++) {
        v += a * vnoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return v;
}

void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec2 p = pixel / u_resolution;

    /* Aspect-corrected sky coords. Centre at horizon (y=0.5). */
    float aspect = u_resolution.x / u_resolution.y;
    vec2 sky = vec2((p.x - 0.5) * aspect, p.y - 0.5);

    /* --- Sky gradient: zenith blue up top, pale at the horizon --- */
    float alt = clamp(p.y, 0.0, 1.0);
    vec3 zenith  = vec3(0.26, 0.48, 0.82);
    vec3 horizon = vec3(0.82, 0.88, 0.95);
    vec3 sky_col = mix(horizon, zenith, smoothstep(0.25, 1.0, alt));

    /* --- Sun: soft disc high and to the right --- */
    vec2 sun_dir = vec2(0.42, 0.32);
    float sd = length(sky - sun_dir);
    vec3 sun_col = vec3(1.0, 0.96, 0.85);
    float sun_disc  = smoothstep(0.055, 0.02, sd);
    float sun_halo  = exp(-sd * 4.0) * 0.35;
    sky_col += sun_col * (sun_disc * 0.9 + sun_halo);

    /* Glow band toward the horizon */
    sky_col += vec3(1.0, 0.85, 0.70) * smoothstep(0.35, 0.0, alt) * 0.15;

    /* --- Flying forward: advance cloud noise in +x, small drift in y --- */
    float t = u_time * 0.08;

    /* Perspective: stretch the cloud field near the horizon so it looks
     * like a distant deck sliding under us. Sample coord uses p.x for
     * across-track and a nonlinear remap of (p.y - horizon) for depth. */
    float depth = max(p.y - 0.48, 0.001);
    float dz    = 0.06 / depth;               /* near = small stride, far = big stride */
    vec2  cp    = vec2(p.x * 1.5 * dz + t, dz * 0.55);

    /* Layer A: main puffy clouds */
    float nA = fbm(cp * 2.3);
    float cloudA = smoothstep(0.48, 0.78, nA);

    /* Layer B: higher thin deck, slower */
    vec2 cp2 = vec2(p.x * 0.9 + t * 0.35, p.y * 0.7 - t * 0.12);
    float nB = fbm(cp2 * 3.0);
    float cloudB = smoothstep(0.58, 0.82, nB) * 0.55;

    /* Clouds only live above the horizon; fade toward it */
    float cloud_mask = smoothstep(0.48, 0.60, p.y);
    float cloud = (cloudA + cloudB * (1.0 - cloudA)) * cloud_mask;

    /* Shade: lit cloud tops (toward sun), shadowed underside */
    vec2 to_sun = normalize(sun_dir - vec2(p.x - 0.5, p.y - 0.5) * vec2(aspect, 1.0));
    float lit = 0.55 + 0.45 * clamp(to_sun.y * 0.6 + to_sun.x * 0.4 + 0.3, 0.0, 1.0);
    float shade = 0.55 + 0.45 * smoothstep(0.45, 0.9, nA);
    vec3 cloud_col = vec3(1.0, 0.98, 0.95) * lit * shade;
    /* Darker bellies near horizon */
    cloud_col *= mix(0.78, 1.0, smoothstep(0.5, 0.9, p.y));

    /* --- Composite --- */
    vec3 col = mix(sky_col, cloud_col, clamp(cloud, 0.0, 1.0));

    /* Very faint ground haze under horizon so the transition isn't harsh */
    float ground = smoothstep(0.48, 0.0, p.y);
    col = mix(col, vec3(0.55, 0.62, 0.72), ground * 0.6);

    color = vec4(col, 1.0);
}
