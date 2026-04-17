/* VGP Background: volumetric cumulus + ground far below.
 *
 * The ray that doesn't hit clouds:
 *   - points up  -> blue sky (zenith gradient + sun + halo)
 *   - points down -> ground: raymarch-lite terrain
 *                   (continents of brown hills / deep blue ocean / haze)
 *
 * Clouds are broken -- lower coverage and a large-scale "continent" mask
 * deliberately leaves sky gaps and ground view-holes.
 *
 * Sun drifts one full orbit per ~20 minutes.
 *
 * Budget worst case ~14 steps * 2 fbm samples * 3 octaves = 336 taps. */

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
float vnoise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash(i),             hash(i + vec2(1, 0)), f.x),
               mix(hash(i + vec2(0,1)), hash(i + vec2(1, 1)), f.x), f.y);
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

/* Cumulus density with a large-scale coverage mask for BREAKS. */
float cloud_density(vec3 p, float time) {
    vec2 xz = p.xz + vec2(time * 0.06, 0.0);

    /* Large-scale "where clouds exist" mask -- creates wide gaps */
    float cover = vnoise(xz * 0.18 + vec2(time * 0.015, 0.0));
    cover = smoothstep(0.38, 0.75, cover);       /* ~35% sky coverage */

    /* Medium detail */
    float base = fbm(xz * 0.9);

    /* Vertical cumulus profile */
    float h = p.y;
    float env = smoothstep(0.32, 0.44, h) * smoothstep(0.72, 0.50, h);

    /* Fine puffy detail */
    float detail = vnoise(xz * 3.2 + vec2(h * 4.0, 0.0));
    float d = (base + detail * 0.25 - 0.50) * env * cover;

    return clamp(d * 2.4, 0.0, 1.0);
}

/* Ground rendering: ocean under sea-level mask, brown terrain above.
 * No raymarching -- we analytically intersect rd with y=0 plane then
 * sample noise at that point. Fog toward horizon. */
vec3 render_ground(vec3 ro, vec3 rd, float time, vec3 sun_dir) {
    /* Only valid if rd.y < 0 */
    float t = -ro.y / rd.y;
    vec2 gp = (ro.xz + rd.xz * t) * 0.15;

    /* Large continents/oceans */
    float land = fbm(gp * 0.25);
    float sea  = smoothstep(0.48, 0.55, land);

    /* Terrain height noise */
    float hills = fbm(gp * 1.1 + vec2(time * 0.01, 0.0));
    float ridges = fbm(gp * 3.3);

    vec3 ocean_col = mix(vec3(0.12, 0.28, 0.48),
                          vec3(0.18, 0.42, 0.60), fbm(gp * 4.0));
    vec3 land_col  = mix(vec3(0.30, 0.26, 0.18),         /* dry dirt */
                          vec3(0.22, 0.32, 0.14), hills); /* green highlands */
    /* Snow on the ridges */
    land_col = mix(land_col, vec3(0.85, 0.88, 0.92),
                    smoothstep(0.55, 0.85, ridges) * smoothstep(0.50, 0.62, land));

    vec3 ground = mix(ocean_col, land_col, sea);

    /* Simple directional lighting from sun (tints warmer / cooler) */
    float lit = 0.55 + 0.45 * clamp(sun_dir.y + 0.3, 0.0, 1.0);
    ground *= lit;

    /* Distance fog: stronger as ray grazes horizon (large t). */
    float fog = 1.0 - exp(-t * 0.04);
    ground = mix(ground, vec3(0.75, 0.82, 0.90), clamp(fog, 0.0, 1.0));

    return ground;
}

void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec2 ndc = (pixel - u_resolution * 0.5) / u_resolution.y;
    float t = u_time;

    /* Camera: flying forward, very slight downward pitch so a band of
     * horizon + ground sits near the bottom of the frame. */
    vec3 ro = vec3(0.0, 0.48, t * 0.045);
    vec3 rd = normalize(vec3(ndc.x * 1.2, ndc.y * 0.85 - 0.10, 1.0));

    /* Slow sun drift */
    float st = t * 0.0052;
    vec3 light_dir = normalize(vec3(cos(st) * 0.65,
                                     0.55 + sin(st * 0.7) * 0.10,
                                     sin(st) * 0.45));

    /* --- Base: sky if looking up, ground if looking down --- */
    vec3 base;
    if (rd.y < -0.002) {
        /* Ground view */
        base = render_ground(ro, rd, t, light_dir);
    } else {
        /* Sky gradient */
        float up = clamp(rd.y * 2.5 + 0.35, 0.0, 1.0);
        vec3 zenith  = vec3(0.20, 0.44, 0.82);
        vec3 horizon = vec3(0.82, 0.88, 0.94);
        base = mix(horizon, zenith, up);

        /* Horizon warmth from sun azimuth */
        float az = max(dot(normalize(vec3(rd.x, 0.0, rd.z)),
                             normalize(vec3(light_dir.x, 0.0, light_dir.z))),
                         0.0);
        base += vec3(1.0, 0.75, 0.55) * pow(az, 4.0) * (1.0 - up) * 0.30;

        /* Sun */
        float sd = max(dot(rd, light_dir), 0.0);
        vec3 sun_tint = vec3(1.0, 0.97, 0.88);
        base += sun_tint * (pow(sd, 700.0) * 2.2 + pow(sd, 8.0) * 0.28);
    }

    /* --- Raymarch the cloud slab --- */
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
        t_enter = 0.0;
        t_exit  = 8.0;
    }
    t_exit = min(t_exit, 8.0);

    vec3 col = base;
    float T = 1.0;

    if (t_exit > t_enter) {
        const int STEPS = 14;
        float span = (t_exit - t_enter) / float(STEPS);
        vec3 p = ro + rd * t_enter;
        vec3 step = rd * span;

        for (int i = 0; i < STEPS; i++) {
            float d = cloud_density(p, t);
            if (d > 0.015) {
                float shade_d = cloud_density(p + light_dir * 0.10, t);
                float lit = exp(-shade_d * 2.2);
                vec3 lit_col = mix(vec3(0.72, 0.76, 0.82),
                                    vec3(1.00, 0.95, 0.85), lit);
                vec3 amb_col = vec3(0.45, 0.55, 0.70);
                vec3 cloud_col = mix(amb_col, lit_col, lit);

                float dT = exp(-d * span * 10.5);
                col += T * (1.0 - dT) * cloud_col;
                T *= dT;
                if (T < 0.02) break;
            }
            p += step;
        }
    }

    /* Tone map -- gentle filmic + gamma */
    col = col / (1.0 + col * 0.8);
    col = pow(col, vec3(1.0 / 2.1));

    color = vec4(col, 1.0);
}
