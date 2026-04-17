/* VGP Background: physically-based volumetric cumulus.
 *
 * Architecture follows Nubis (Guerrilla / Schneider) + Heckel's cloudscape
 * raymarch tutorial:
 *   1. Coverage mask at large scale -> real sky breaks
 *   2. Cumulus vertical envelope (flat bottom, rounded top)
 *   3. High-frequency erosion for wispy edges
 *   4. Nested 4-step light march toward the sun
 *   5. Henyey-Greenstein dual-lobe phase (silver-lining near sun)
 *   6. Powder approximation (bright edges when backlit)
 *   7. Ambient sky-fill floor (shadow sides aren't black)
 *   8. Ground layer below horizon -- ocean / continents / snow
 *
 * Budget: 18 main steps, cloud-free rays short-circuit, worst case
 * ~860 taps / pixel, average ~300. Sun drifts one orbit in ~20 min. */

#define PI 3.14159265

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i),              hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0, 1)), hash(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.55;
    for (int i = 0; i < 3; i++) {
        v += a * vnoise(p);
        p = p * 2.03 + vec2(1.3, 2.7);
        a *= 0.5;
    }
    return v;
}

/* Henyey-Greenstein phase function. Forward-scattering (g>0) makes the
 * cloud around the sun glow -- the "silver lining". Dual-lobe variant
 * adds a gentle back-glow so the anti-sun side isn't flat. */
float hg(float cos_t, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * PI * pow(1.0 + g2 - 2.0 * g * cos_t, 1.5));
}
float phase(float cos_t) {
    return mix(hg(cos_t, 0.65), hg(cos_t, -0.25), 0.30);
}

/* Cloud density = coverage * cumulus shape * erosion. */
float cloud_density(vec3 p, float time) {
    vec2 xz = p.xz + vec2(time * 0.06, 0.0);

    /* Weather / coverage map -- large-scale noise gates cloud presence.
     * This is what produces the visible sky breaks. */
    float cover = vnoise(xz * 0.14 + vec2(time * 0.012, 0.0));
    cover = smoothstep(0.44, 0.80, cover);

    /* Base cumulus shape */
    float base = fbm(xz * 0.85);

    /* Vertical envelope: flat-bottom + rounded-top (cumulus profile) */
    float h = p.y;
    float env = smoothstep(0.30, 0.43, h) * smoothstep(0.72, 0.48, h);

    float shape = base * env * cover;
    shape = smoothstep(0.28, 0.80, shape);

    /* Erosion: high-frequency noise bites into the shape for wispy
     * flocculent edges. Follows Nubis "billow + wispy" idea. */
    if (shape > 0.001) {
        float erosion = fbm(xz * 4.6 + vec2(h * 3.0, time * 0.15));
        shape -= (1.0 - shape) * erosion * 0.55;
    }
    return clamp(shape, 0.0, 1.0);
}

/* Short raymarch along the sun direction to estimate transmittance. */
float light_march(vec3 p, vec3 sun_dir, float time) {
    const int STEPS = 4;
    float step_len = 0.08;
    float total = 0.0;
    for (int i = 0; i < STEPS; i++) {
        vec3 sp = p + sun_dir * step_len * (float(i) + 0.5);
        total += cloud_density(sp, time);
    }
    return exp(-total * step_len * 2.6);
}

/* Ground intersection at y=0: ocean / brown land / snowy ridges +
 * distance fog. Only called when the ray actually points down. */
vec3 render_ground(vec3 ro, vec3 rd, float time, vec3 sun_dir) {
    float t = -ro.y / rd.y;
    vec2 gp = (ro.xz + rd.xz * t) * 0.15;

    float land = fbm(gp * 0.25);
    float sea  = smoothstep(0.48, 0.55, land);
    float hills  = fbm(gp * 1.1 + vec2(time * 0.01, 0.0));
    float ridges = fbm(gp * 3.3);

    vec3 ocean = mix(vec3(0.10, 0.26, 0.46),
                      vec3(0.16, 0.40, 0.58), fbm(gp * 4.0));
    vec3 land_col = mix(vec3(0.28, 0.24, 0.16),
                        vec3(0.20, 0.30, 0.12), hills);
    land_col = mix(land_col, vec3(0.85, 0.88, 0.92),
                    smoothstep(0.60, 0.85, ridges) * smoothstep(0.52, 0.62, land));

    vec3 ground = mix(ocean, land_col, sea);
    float lit = 0.55 + 0.45 * clamp(sun_dir.y + 0.3, 0.0, 1.0);
    ground *= lit;

    float fog = 1.0 - exp(-t * 0.05);
    ground = mix(ground, vec3(0.75, 0.82, 0.90), clamp(fog, 0.0, 1.0));
    return ground;
}

void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec2 ndc = (pixel - u_resolution * 0.5) / u_resolution.y;
    float t = u_time;

    /* Camera: flying forward, slight pitch down */
    vec3 ro = vec3(0.0, 0.48, t * 0.045);
    vec3 rd = normalize(vec3(ndc.x * 1.2, ndc.y * 0.85 - 0.10, 1.0));

    /* Slow sun drift -- one orbit per ~20 minutes */
    float st = t * 0.0052;
    vec3 sun_dir = normalize(vec3(cos(st) * 0.65,
                                    0.55 + sin(st * 0.7) * 0.10,
                                    sin(st) * 0.45));
    vec3 sun_col = vec3(1.0, 0.96, 0.82);

    /* Base: sky above, ground below */
    vec3 base;
    if (rd.y < -0.002) {
        base = render_ground(ro, rd, t, sun_dir);
    } else {
        float up = clamp(rd.y * 2.5 + 0.35, 0.0, 1.0);
        vec3 zenith  = vec3(0.18, 0.42, 0.80);
        vec3 horizon = vec3(0.82, 0.88, 0.94);
        base = mix(horizon, zenith, up);

        float az = max(dot(normalize(vec3(rd.x, 0.0, rd.z)),
                             normalize(vec3(sun_dir.x, 0.0, sun_dir.z))), 0.0);
        base += vec3(1.0, 0.75, 0.55) * pow(az, 4.0) * (1.0 - up) * 0.30;

        float sd = max(dot(rd, sun_dir), 0.0);
        base += sun_col * (pow(sd, 700.0) * 2.4 + pow(sd, 8.0) * 0.28);
    }

    /* Cloud slab bounds */
    const float y_bot = 0.30;
    const float y_top = 0.72;
    float t_enter, t_exit;
    if (rd.y > 0.001) {
        t_enter = max((y_bot - ro.y) / rd.y, 0.0);
        t_exit  = (y_top - ro.y) / rd.y;
    } else if (rd.y < -0.001) {
        t_enter = max((y_top - ro.y) / rd.y, 0.0);
        t_exit  = (y_bot - ro.y) / rd.y;
    } else {
        t_enter = 0.0; t_exit = 8.0;
    }
    t_exit = min(t_exit, 8.0);

    vec3 col = base;
    float T = 1.0;

    if (t_exit > t_enter) {
        const int STEPS = 18;
        float span = (t_exit - t_enter) / float(STEPS);
        vec3 p = ro + rd * t_enter;
        vec3 step = rd * span;
        float ph = phase(dot(rd, sun_dir));

        for (int i = 0; i < STEPS; i++) {
            float d = cloud_density(p, t);
            if (d > 0.010) {
                /* Transmittance along the light ray to this sample */
                float sun_T = light_march(p, sun_dir, t);

                /* Powder: backlit edges glow brighter (physics hack) */
                float powder = 1.0 - exp(-d * 2.0);

                /* In-scattered radiance from the sun through cloud */
                vec3 lit = sun_col * sun_T * ph * powder * 3.0;

                /* Ambient sky fill so shadow sides aren't black */
                vec3 ambient = vec3(0.50, 0.62, 0.78) * (0.25 + powder * 0.15);

                vec3 scatter = lit + ambient;

                /* Beer-Lambert extinction for this slab segment */
                float dT = exp(-d * span * 9.0);
                col += T * (1.0 - dT) * scatter;
                T *= dT;
                if (T < 0.02) break;
            }
            p += step;
        }
    }

    /* Film tonemap + gamma */
    col = col / (1.0 + col * 0.82);
    col = pow(col, vec3(1.0 / 2.1));

    color = vec4(col, 1.0);
}
