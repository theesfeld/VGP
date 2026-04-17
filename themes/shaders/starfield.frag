/* VGP Background: lightweight starfield.
 * Tile-hashed cells instead of a big per-pixel loop.
 * Must stay cheap -- ran at native display res every frame. */
void effect(out vec4 color, in vec2 uv, in vec2 pixel) {
    vec2 p = pixel / u_resolution;
    /* Correct for aspect so stars stay round */
    vec2 ap = p * vec2(u_resolution.x / u_resolution.y, 1.0);

    /* Coarse cells -- each cell hosts at most one star */
    float cells = 60.0;
    vec2 cp = ap * cells;
    vec2 cid = floor(cp);
    vec2 cf  = fract(cp);

    /* Deterministic hash from cell id */
    float h1 = fract(sin(dot(cid, vec2(127.1, 311.7))) * 43758.5453);
    float h2 = fract(sin(dot(cid, vec2(269.5, 183.3))) * 43758.5453);
    float h3 = fract(sin(dot(cid, vec2( 74.7,  12.9))) * 43758.5453);

    /* Only ~25% of cells contain a star */
    float present = step(0.75, h3);

    vec2 star_pos = vec2(h1, h2);
    float d = length(cf - star_pos);

    /* Brightness and size hashed from cell */
    float br = h3 * h3;
    float size = 0.04 + br * 0.08;
    float tw = 0.7 + 0.3 * sin(u_time * (0.3 + br * 1.5) + h1 * 6.28);
    float star = smoothstep(size, 0.0, d) * br * tw * present;

    vec3 col = vec3(0.9, 0.88, 0.85) * star;
    color = vec4(col, 1.0);
}
