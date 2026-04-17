/* VGP Background: volumetric cumulus, flying forward, slow sun drift.
 *
 * Technique: raymarch a horizontal cloud slab with Beer-Lambert extinction.
 *   - Density: 2D fBM (xz) shaped by a vertical profile -> cheap 2.5D clouds
 *   - Per-sample "light march": one forward sample toward the sun for
 *     self-shadowing -- darkens cloud undersides naturally
 *   - Front-to-back compositing with early termination at low transmittance
 *
 * Budget (worst case per pixel):
 *   14 main steps * (1 density + 1 light sample) * fBM(3 octaves, 4 taps)
 *   = 14 * 2 * 12 = 336 noise taps. Safe at 3440x1440 @ 60fps.
 *
 * Sun drifts one full orbit every ~20 minutes. */

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

/* 3-octave fBM -- fixed iteration count */
float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.55;
    for (int i = 0; i < 3; i++) {
        v += a * vnoise(p);
        p = p * 2.03 + vec2(1.3, 2.7);
        a *= 0.5;
    }
    return v;
}

/* Cloud slab sits between y=0.30 and y=0.70. Density shaped by fBM in the
 * horizontal plane and feathered top/bottom by a smooth vertical profile. */
float cloud_density(vec3 p, float time) {
    vec2 xz = p.xz + vec2(time * 0.06, 0.0);
    float base = fbm(xz * 0.9);

    /* Vertical envelope -- cumulus: flat bottom, rounded top */
    float h = p.y;
    float env = smoothstep(0.30, 0.42, h) * smoothstep(0.72, 0.50, h);

    /* Puffy detail: modulate base with a higher frequency noise keyed by height */
    float detail = vnoise(xz * 3.2 + vec2(h * 4.0, 0.0));
    float d = (base + detail * 0.25 - 0.45) * env;

    /* Sharp but fluffy edges */
    return clamp(d * 2.2, 0.0, 1.0);
}

void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    /* --- Ray setup ---
     * Screen-space p in [-aspect/2, aspect/2] x [-0.5, 0.5]. */
    vec2 ndc = (pixel - u_resolution * 0.5) / u_resolution.y;
    float t = u_time;

    /* Forward flight along +z. Small pitch down so the horizon sits
     * near image center. */
    vec3 ro = vec3(0.0, 0.48, t * 0.045);
    vec3 rd = normalize(vec3(ndc.x * 1.2, ndc.y * 0.75 - 0.02, 1.0));

    /* --- Slowly drifting sun ---
     * One orbit in ~1200s. Elevation varies slowly too. */
    float st = t * 0.0052;
    vec3 light_dir = normalize(vec3(cos(st) * 0.65,
                                     0.55 + sin(st * 0.7) * 0.10,
                                     sin(st) * 0.45));

    /* --- Sky gradient --- */
    float up = clamp(rd.y * 2.0 + 0.45, 0.0, 1.0);
    vec3 zenith  = vec3(0.22, 0.45, 0.82);
    vec3 horizon = vec3(0.82, 0.88, 0.94);
    vec3 sky = mix(horizon, zenith, up);

    /* Horizon warmth picks up the sun's azimuth */
    float sun_az = max(dot(normalize(vec3(rd.x, 0.0, rd.z)),
                            normalize(vec3(light_dir.x, 0.0, light_dir.z))),
                        0.0);
    sky += vec3(1.0, 0.75, 0.55) * pow(sun_az, 4.0) * (1.0 - up) * 0.25;

    /* Sun: tight disc + broader halo */
    float sd = max(dot(rd, light_dir), 0.0);
    vec3  sun_tint = vec3(1.0, 0.97, 0.88);
    sky += sun_tint * (pow(sd, 700.0) * 2.2 + pow(sd, 8.0) * 0.28);

    /* --- Raymarch the cloud slab --- */
    /* Enter/exit heights */
    const float y_bot = 0.30;
    const float y_top = 0.72;

    /* If ray points down/up and won't cross the slab, skip */
    float t_enter, t_exit;
    if (rd.y > 0.001) {
        t_enter = max((y_bot - ro.y) / rd.y, 0.0);
        t_exit  = (y_top - ro.y) / rd.y;
    } else if (rd.y < -0.001) {
        t_enter = max((y_top - ro.y) / rd.y, 0.0);
        t_exit  = (y_bot - ro.y) / rd.y;
    } else {
        t_enter = 0.0;
        t_exit  = 8.0;
    }
    t_exit = min(t_exit, 8.0);

    vec3  col = sky;
    float T   = 1.0;   /* transmittance */

    if (t_exit > t_enter) {
        const int STEPS = 14;
        float span = (t_exit - t_enter) / float(STEPS);
        vec3 p = ro + rd * t_enter;
        vec3 step = rd * span;

        for (int i = 0; i < STEPS; i++) {
            float d = cloud_density(p, t);
            if (d > 0.015) {
                /* Self-shadow: 1 sample further along the light ray.
                 * Cheap but effective -- darker on the far side from sun. */
                float shade_d = cloud_density(p + light_dir * 0.10, t);
                float lit = exp(-shade_d * 2.2);

                /* Cloud albedo: warm-lit top, cool-shadowed bottom */
                vec3 lit_col  = mix(vec3(0.72, 0.76, 0.82),
                                     vec3(1.00, 0.95, 0.85), lit);
                /* Ambient blue skylight on the shadow side */
                vec3 amb_col  = vec3(0.45, 0.55, 0.70);
                vec3 cloud_col = mix(amb_col, lit_col, lit);

                /* Beer-Lambert extinction for this slab segment */
                float dT = exp(-d * span * 10.5);
                col += T * (1.0 - dT) * cloud_col;
                T *= dT;
                if (T < 0.02) break;
            }
            p += step;
        }
    }

    /* Ground haze under horizon so the bottom strip isn't harsh */
    float ground = smoothstep(0.0, -0.12, ndc.y);
    col = mix(col, vec3(0.55, 0.62, 0.72), ground * 0.7);

    /* Tone map -- gentle film-ish curve + gamma */
    col = col / (1.0 + col * 0.8);
    col = pow(col, vec3(1.0 / 2.1));

    color = vec4(col, 1.0);
}
