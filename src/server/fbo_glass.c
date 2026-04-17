/* SPDX-License-Identifier: MIT */
/* FBO Glass Pipeline -- scene FBO + downsample chain + per-window
 * glass fragment shader with Fresnel, IOR offset, chromatic aberration.
 *
 * Phase contract: all GL happens between nvgEndFrame and nvgBeginFrame.
 * After this runs, the default framebuffer holds:
 *   - The full scene (from scene_fbo) as base layer
 *   - Glass overlays where windows live (sampling blurred scene)
 * NanoVG then resumes and draws chrome / text / widgets on top. */

#ifdef VGP_HAS_GPU_BACKEND

#include "fbo_glass.h"
#include "vgp/log.h"

#include <stdlib.h>

#define TAG "glass"

#define BLUR_HALF_DIV    2
#define BLUR_QUARTER_DIV 4

/* ============================================================
 * GLSL shaders
 * ============================================================ */

/* Shared vertex shader: passes through NDC + UV. */
static const char *vs_passthrough =
    "#version 300 es\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

/* Pass-through: samples a texture and writes it. Used to blit
 * scene_fbo onto the default framebuffer as the base layer. */
static const char *fs_passthrough =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = vec4(texture(u_tex, v_uv).rgb, 1.0);\n"
    "}\n";

/* Box-blur downsample: 5-tap cross kernel. Cheap but perceptually
 * Gaussian-ish at the small blur amounts we need for glass. */
static const char *fs_downsample =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D u_tex;\n"
    "uniform vec2 u_texel;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec3 c = texture(u_tex, v_uv).rgb * 0.4;\n"
    "    c += texture(u_tex, v_uv + vec2( u_texel.x, 0.0)).rgb * 0.15;\n"
    "    c += texture(u_tex, v_uv + vec2(-u_texel.x, 0.0)).rgb * 0.15;\n"
    "    c += texture(u_tex, v_uv + vec2(0.0,  u_texel.y)).rgb * 0.15;\n"
    "    c += texture(u_tex, v_uv + vec2(0.0, -u_texel.y)).rgb * 0.15;\n"
    "    frag_color = vec4(c, 1.0);\n"
    "}\n";

/* Glass fragment: the main event.
 *   Samples the blurred scene at a refracted UV, applies chromatic
 *   aberration proportional to edge distance, Fresnel edge glow,
 *   top-edge light scatter, rounded-rect SDF alpha, focus halo. */
static const char *fs_glass =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D u_scene;\n"
    "uniform sampler2D u_blur;\n"
    "uniform vec4  u_rect;\n"        /* x, y, w, h in pixels */
    "uniform vec2  u_resolution;\n"
    "uniform float u_corner_radius;\n"
    "uniform vec4  u_tint;\n"
    "uniform vec3  u_accent;\n"
    "uniform float u_focused;\n"     /* 0 / 1 */
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "\n"
    "float rbox_sdf(vec2 p, vec2 b, float r) {\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return length(max(q, 0.0)) - r;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 pix = vec2(v_uv.x, 1.0 - v_uv.y) * u_resolution;\n"
    "    vec2 rect_xy = u_rect.xy;\n"
    "    vec2 rect_wh = u_rect.zw;\n"
    "    vec2 rc = rect_xy + rect_wh * 0.5;\n"
    "    vec2 hs = rect_wh * 0.5;\n"
    "    float d = rbox_sdf(pix - rc, hs, u_corner_radius);\n"
    "    float alpha = 1.0 - smoothstep(-1.0, 1.0, d);\n"
    "    if (alpha < 0.003) discard;\n"
    "\n"
    "    vec2 local = (pix - rect_xy) / rect_wh;\n"
    "    vec2 from_centre = local - 0.5;\n"
    "    float edge = max(abs(from_centre.x), abs(from_centre.y)) * 2.0;\n"
    "    vec2 dir = length(from_centre) > 0.001 ?\n"
    "                normalize(from_centre) : vec2(0.0, 0.0);\n"
    "\n"
    "    /* Scene UV (remember scene texture has V=0 at bottom). */\n"
    "    vec2 scene_uv = v_uv;\n"
    "\n"
    "    /* Refraction offset: push sample AWAY from centre; magnitude\n"
    "     * grows near the edge. This fakes IOR bending. */\n"
    "    float refr = 0.005 + edge * 0.012;\n"
    "    scene_uv -= dir * refr;\n"
    "\n"
    "    /* Chromatic aberration: R and B channels shifted opposite\n"
    "     * along the edge-outward direction. */\n"
    "    float ca = edge * edge * 0.004;\n"
    "    float r = texture(u_blur, scene_uv + dir * ca).r;\n"
    "    float g = texture(u_blur, scene_uv).g;\n"
    "    float b = texture(u_blur, scene_uv - dir * ca).b;\n"
    "    vec3 col = vec3(r, g, b);\n"
    "\n"
    "    /* Glass tint (cool blue plexi) */\n"
    "    col = mix(col, u_tint.rgb, u_tint.a);\n"
    "\n"
    "    /* Top-edge light scatter -- light pools at the top of plexi */\n"
    "    float top = smoothstep(0.18, 0.0, local.y);\n"
    "    col += vec3(1.0) * top * 0.22;\n"
    "\n"
    "    /* Fresnel-like edge brightness */\n"
    "    float fres = pow(edge, 3.0);\n"
    "    col += vec3(1.0) * fres * 0.28;\n"
    "\n"
    "    /* Focus halo (yellow accent when focused) */\n"
    "    if (u_focused > 0.5) {\n"
    "        float halo = smoothstep(0.85, 1.0, edge);\n"
    "        col += u_accent * halo * 0.35;\n"
    "    }\n"
    "\n"
    "    frag_color = vec4(col, alpha);\n"
    "}\n";

/* ============================================================
 * GL helpers
 * ============================================================ */

static GLuint compile_shader(GLenum type, const char *src, const char *label)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        VGP_LOG_ERROR(TAG, "%s shader compile failed: %s", label, log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs, const char *label)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), NULL, log);
        VGP_LOG_ERROR(TAG, "%s program link failed: %s", label, log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

static GLuint build_program(const char *vs_src, const char *fs_src,
                              const char *label)
{
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src, label);
    if (!vs) return 0;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, label);
    if (!fs) { glDeleteShader(vs); return 0; }
    GLuint p = link_program(vs, fs, label);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

static int make_fbo(GLuint *fbo, GLuint *tex,
                     uint32_t w, uint32_t h, const char *label)
{
    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h,
                  0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, *tex, 0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        VGP_LOG_ERROR(TAG, "%s FBO incomplete: 0x%x", label, status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return -1;
    }
    VGP_LOG_INFO(TAG, "%s FBO %ux%u ready", label, w, h);
    return 0;
}

static void destroy_fbo(GLuint *fbo, GLuint *tex)
{
    if (*fbo) { glDeleteFramebuffers(1, fbo); *fbo = 0; }
    if (*tex) { glDeleteTextures(1, tex); *tex = 0; }
}

/* ============================================================
 * Init / resize / destroy
 * ============================================================ */

int vgp_fbo_glass_init(vgp_gpu_state_t *gs, uint32_t w, uint32_t h)
{
    /* Programs */
    gs->gl_prog_passthrough = build_program(vs_passthrough, fs_passthrough, "passthrough");
    gs->gl_prog_downsample  = build_program(vs_passthrough, fs_downsample,  "downsample");
    gs->gl_prog_glass       = build_program(vs_passthrough, fs_glass,       "glass");
    if (!gs->gl_prog_passthrough || !gs->gl_prog_downsample || !gs->gl_prog_glass) {
        VGP_LOG_ERROR(TAG, "shader build failed -- FBO pipeline disabled");
        return -1;
    }

    /* Fullscreen quad: two triangles covering NDC. */
    static const float quad[] = {
        /* pos      uv */
        -1.f, -1.f,  0.f, 0.f,
         1.f, -1.f,  1.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
        -1.f, -1.f,  0.f, 0.f,
         1.f,  1.f,  1.f, 1.f,
        -1.f,  1.f,  0.f, 1.f,
    };
    glGenVertexArrays(1, &gs->gl_quad_vao);
    glGenBuffers(1, &gs->gl_quad_vbo);
    glBindVertexArray(gs->gl_quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gs->gl_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);

    gs->fbo_enabled = false;   /* set true by resize on success */
    return vgp_fbo_glass_resize(gs, w, h);
}

int vgp_fbo_glass_resize(vgp_gpu_state_t *gs, uint32_t w, uint32_t h)
{
    if (gs->scene_w == w && gs->scene_h == h && gs->fbo_enabled)
        return 0;

    destroy_fbo(&gs->scene_fbo,  &gs->scene_tex);
    destroy_fbo(&gs->blur_h_fbo, &gs->blur_h_tex);
    destroy_fbo(&gs->blur_q_fbo, &gs->blur_q_tex);

    uint32_t hw = w / BLUR_HALF_DIV;
    uint32_t hh = h / BLUR_HALF_DIV;
    uint32_t qw = w / BLUR_QUARTER_DIV;
    uint32_t qh = h / BLUR_QUARTER_DIV;
    if (hw < 1) hw = 1;
    if (hh < 1) hh = 1;
    if (qw < 1) qw = 1;
    if (qh < 1) qh = 1;

    if (make_fbo(&gs->scene_fbo,  &gs->scene_tex,  w,  h,  "scene")  < 0 ||
        make_fbo(&gs->blur_h_fbo, &gs->blur_h_tex, hw, hh, "blur/2") < 0 ||
        make_fbo(&gs->blur_q_fbo, &gs->blur_q_tex, qw, qh, "blur/4") < 0) {
        gs->fbo_enabled = false;
        return -1;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gs->scene_w = w;  gs->scene_h = h;
    gs->blur_hw = hw; gs->blur_hh = hh;
    gs->blur_qw = qw; gs->blur_qh = qh;
    gs->fbo_enabled = true;
    return 0;
}

void vgp_fbo_glass_destroy(vgp_gpu_state_t *gs)
{
    destroy_fbo(&gs->scene_fbo,  &gs->scene_tex);
    destroy_fbo(&gs->blur_h_fbo, &gs->blur_h_tex);
    destroy_fbo(&gs->blur_q_fbo, &gs->blur_q_tex);
    if (gs->gl_quad_vao) { glDeleteVertexArrays(1, &gs->gl_quad_vao); gs->gl_quad_vao = 0; }
    if (gs->gl_quad_vbo) { glDeleteBuffers(1, &gs->gl_quad_vbo); gs->gl_quad_vbo = 0; }
    if (gs->gl_prog_passthrough) { glDeleteProgram(gs->gl_prog_passthrough); gs->gl_prog_passthrough = 0; }
    if (gs->gl_prog_downsample)  { glDeleteProgram(gs->gl_prog_downsample);  gs->gl_prog_downsample  = 0; }
    if (gs->gl_prog_glass)       { glDeleteProgram(gs->gl_prog_glass);       gs->gl_prog_glass       = 0; }
    gs->fbo_enabled = false;
}

bool vgp_fbo_glass_enabled(const vgp_gpu_state_t *gs)
{
    return gs->fbo_enabled;
}

/* ============================================================
 * Composite
 * ============================================================ */

static void draw_fullscreen_quad(vgp_gpu_state_t *gs)
{
    glBindVertexArray(gs->gl_quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* Run the user-supplied scene shader into scene_fbo. The scene shader
 * is the existing cloud/sky background shader managed by shader_mgr;
 * we just bind our FBO, set its uniforms, draw fullscreen, unbind. */
static void render_scene_to_fbo(vgp_gpu_state_t *gs,
                                  GLuint scene_program,
                                  float u_time,
                                  uint32_t w, uint32_t h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, gs->scene_fbo);
    glViewport(0, 0, (GLsizei)w, (GLsizei)h);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(scene_program);
    GLint loc;
    loc = glGetUniformLocation(scene_program, "u_time");
    if (loc >= 0) glUniform1f(loc, u_time);
    loc = glGetUniformLocation(scene_program, "u_resolution");
    if (loc >= 0) glUniform2f(loc, (float)w, (float)h);
    loc = glGetUniformLocation(scene_program, "u_rect");
    if (loc >= 0) glUniform4f(loc, 0.0f, 0.0f, (float)w, (float)h);
    loc = glGetUniformLocation(scene_program, "u_mouse");
    if (loc >= 0) glUniform2f(loc, 0.0f, 0.0f);
    loc = glGetUniformLocation(scene_program, "u_window_count");
    if (loc >= 0) glUniform1i(loc, 0);

    draw_fullscreen_quad(gs);
    glBindVertexArray(0);
    glUseProgram(0);
}

static void downsample(vgp_gpu_state_t *gs,
                        GLuint src_tex, float src_w, float src_h,
                        GLuint dst_fbo, uint32_t dst_w, uint32_t dst_h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, dst_fbo);
    glViewport(0, 0, (GLsizei)dst_w, (GLsizei)dst_h);
    glUseProgram(gs->gl_prog_downsample);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, src_tex);
    glUniform1i(glGetUniformLocation(gs->gl_prog_downsample, "u_tex"), 0);
    glUniform2f(glGetUniformLocation(gs->gl_prog_downsample, "u_texel"),
                 1.0f / src_w, 1.0f / src_h);
    draw_fullscreen_quad(gs);
}

static void blit_scene_to_default(vgp_gpu_state_t *gs,
                                     uint32_t vp_w, uint32_t vp_h)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei)vp_w, (GLsizei)vp_h);
    glDisable(GL_BLEND);
    glUseProgram(gs->gl_prog_passthrough);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gs->scene_tex);
    glUniform1i(glGetUniformLocation(gs->gl_prog_passthrough, "u_tex"), 0);
    draw_fullscreen_quad(gs);
}

static void draw_glass_rect(vgp_gpu_state_t *gs,
                              const vgp_glass_rect_t *r,
                              uint32_t vp_w, uint32_t vp_h,
                              float tr, float tg, float tb, float ta,
                              float ar, float ag, float ab)
{
    glUseProgram(gs->gl_prog_glass);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gs->scene_tex);
    glUniform1i(glGetUniformLocation(gs->gl_prog_glass, "u_scene"), 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gs->blur_q_tex);
    glUniform1i(glGetUniformLocation(gs->gl_prog_glass, "u_blur"), 1);

    glUniform2f(glGetUniformLocation(gs->gl_prog_glass, "u_resolution"),
                 (float)vp_w, (float)vp_h);
    glUniform4f(glGetUniformLocation(gs->gl_prog_glass, "u_rect"),
                 (float)r->x, (float)r->y, (float)r->w, (float)r->h);
    glUniform1f(glGetUniformLocation(gs->gl_prog_glass, "u_corner_radius"),
                 r->corner_radius);
    glUniform4f(glGetUniformLocation(gs->gl_prog_glass, "u_tint"),
                 tr, tg, tb, ta);
    glUniform3f(glGetUniformLocation(gs->gl_prog_glass, "u_accent"),
                 ar, ag, ab);
    glUniform1f(glGetUniformLocation(gs->gl_prog_glass, "u_focused"),
                 r->focused ? 1.0f : 0.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    draw_fullscreen_quad(gs);
}

void vgp_fbo_glass_composite(vgp_gpu_state_t *gs,
                               unsigned int scene_program,
                               float u_time,
                               uint32_t vp_w, uint32_t vp_h,
                               const vgp_glass_rect_t *rects, int rect_count,
                               float tint_r, float tint_g, float tint_b,
                               float tint_a,
                               float ar, float ag, float ab)
{
    if (!gs->fbo_enabled || !scene_program) return;

    if (gs->scene_w != vp_w || gs->scene_h != vp_h)
        vgp_fbo_glass_resize(gs, vp_w, vp_h);

    /* Save prior GL error state; anything bad after here is ours. */
    while (glGetError() != GL_NO_ERROR) { /* drain */ }

    /* 1. Scene -> scene_fbo */
    render_scene_to_fbo(gs, scene_program, u_time, vp_w, vp_h);

    /* 2. Downsample chain: scene -> half -> quarter */
    downsample(gs, gs->scene_tex,  (float)vp_w, (float)vp_h,
                gs->blur_h_fbo, gs->blur_hw, gs->blur_hh);
    downsample(gs, gs->blur_h_tex, (float)gs->blur_hw, (float)gs->blur_hh,
                gs->blur_q_fbo, gs->blur_qw, gs->blur_qh);

    /* 3. Scene into default framebuffer as base */
    blit_scene_to_default(gs, vp_w, vp_h);

    /* 4. Glass overlay per window */
    for (int i = 0; i < rect_count; i++) {
        draw_glass_rect(gs, &rects[i], vp_w, vp_h,
                          tint_r, tint_g, tint_b, tint_a,
                          ar, ag, ab);
    }

    /* Reset to defaults NanoVG expects */
    glUseProgram(0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, (GLsizei)vp_w, (GLsizei)vp_h);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        VGP_LOG_WARN(TAG, "composite trailing GL error 0x%x", err);
}

#endif /* VGP_HAS_GPU_BACKEND */