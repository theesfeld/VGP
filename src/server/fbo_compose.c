/* FBO Compositing Pipeline -- Photorealistic Glass Effects
 * Captures the rendered scene to a texture, then composites
 * blurred/distorted versions as window glass backgrounds.
 * Gaussian blur + chromatic aberration + refraction distortion. */

#ifdef VGP_HAS_GPU_BACKEND

#include "fbo_compose.h"
#include "vgp/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "fbo"

/* ============================================================
 * Blur + Glass shader (GLSL ES 3.0)
 * ============================================================ */

static const char *blur_vert_src =
    "#version 300 es\n"
    "layout(location=0) in vec2 a_pos;\n"
    "layout(location=1) in vec2 a_uv;\n"
    "out vec2 v_uv;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_pos, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "}\n";

/* Photorealistic glass fragment shader:
 * - Multi-pass box blur approximating gaussian
 * - Chromatic aberration (RGB channels offset slightly)
 * - Refraction distortion (UV offset based on normal)
 * - Frosted glass grain noise
 * - Tint overlay */
/* Simplified glass blur shader -- fixed loop bounds for GLES3 compat */
static const char *blur_frag_src =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D u_scene;\n"
    "uniform vec4 u_rect;\n"         /* x,y,w,h in NDC */
    "uniform vec2 u_resolution;\n"
    "uniform float u_blur_radius;\n"
    "uniform vec4 u_tint;\n"
    "uniform float u_corner_radius;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "\n"
    "float rand(vec2 co) {\n"
    "    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);\n"
    "}\n"
    "\n"
    "/* Rounded rect SDF for clipping */\n"
    "float roundedBox(vec2 p, vec2 b, float r) {\n"
    "    vec2 q = abs(p) - b + r;\n"
    "    return length(max(q, 0.0)) - r;\n"
    "}\n"
    "\n"
    "void main() {\n"
    "    vec2 uv = v_uv;\n"
    "    vec2 pixel_size = 1.0 / u_resolution;\n"
    "    float radius = u_blur_radius;\n"
    "\n"
    "    /* Refraction distortion: slight UV warp like looking through glass */\n"
    "    vec2 center = vec2(0.5);\n"
    "    vec2 offset = (uv - center) * 0.015;\n"  /* barrel distortion */
    "    uv += offset * radius * 0.1;\n"
    "\n"
    "    /* Frosted glass noise: per-pixel UV jitter */\n"
    "    float noise = rand(uv * u_resolution * 0.1) * 2.0 - 1.0;\n"
    "    uv += vec2(noise) * pixel_size * radius * 0.3;\n"
    "\n"
    "    /* Multi-sample blur (9-tap box filter, 3 passes approximation) */\n"
    "    vec3 blur = vec3(0.0);\n"
    "    float total = 0.0;\n"
    "    int samples = int(min(radius, 8.0));\n"
    "    if (samples < 1) samples = 1;\n"
    "    for (int ix = -samples; ix <= samples; ix++) {\n"
    "        for (int iy = -samples; iy <= samples; iy++) {\n"
    "            vec2 off = vec2(float(ix), float(iy)) * pixel_size * (radius / float(samples));\n"
    "            float w = 1.0 / (1.0 + float(ix*ix + iy*iy) * 0.5);\n"
    "            blur += texture(u_scene, uv + off).rgb * w;\n"
    "            total += w;\n"
    "        }\n"
    "    }\n"
    "    blur /= total;\n"
    "\n"
    "    /* Chromatic aberration: offset R and B channels slightly */\n"
    "    float ca_strength = radius * 0.0008;\n"
    "    float r_off = texture(u_scene, uv + vec2(ca_strength, 0.0)).r;\n"
    "    float b_off = texture(u_scene, uv - vec2(ca_strength, 0.0)).b;\n"
    "    blur.r = mix(blur.r, r_off, 0.3);\n"
    "    blur.b = mix(blur.b, b_off, 0.3);\n"
    "\n"
    "    /* Apply tint (plexiglass color/opacity) */\n"
    "    vec3 glass = mix(blur, u_tint.rgb, u_tint.a);\n"
    "\n"
    "    /* Subtle glass grain texture */\n"
    "    float grain = rand(uv * u_resolution) * 0.02;\n"
    "    glass += grain;\n"
    "\n"
    "    /* Edge highlight (Fresnel-like: brighter at edges of glass) */\n"
    "    vec2 norm_pos = (v_uv - vec2(0.5)) * 2.0;\n"
    "    float edge = length(norm_pos);\n"
    "    float fresnel = smoothstep(0.5, 1.2, edge) * 0.08;\n"
    "    glass += fresnel;\n"
    "\n"
    "    /* Rounded corner clip */\n"
    "    vec2 pix = v_uv * u_resolution;\n"
    "    vec2 rect_center = u_rect.xy + u_rect.zw * 0.5;\n"
    "    vec2 half_size = u_rect.zw * 0.5;\n"
    "    float d = roundedBox(pix - rect_center, half_size, u_corner_radius);\n"
    "    float alpha = 1.0 - smoothstep(-1.0, 1.0, d);\n"
    "\n"
    "    frag_color = vec4(glass, alpha * 0.92);\n"
    "}\n";

/* Simplified glass shader for reliability */
static const char *blur_frag_simple =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D u_scene;\n"
    "uniform vec2 u_resolution;\n"
    "uniform float u_blur_radius;\n"
    "uniform vec4 u_tint;\n"
    "in vec2 v_uv;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    vec2 uv = v_uv;\n"
    "    vec2 ps = 1.0 / u_resolution;\n"
    "    float r = u_blur_radius;\n"
    "    vec3 c = vec3(0.0);\n"
    "    float tw = 0.0;\n"
    "    for (int ix = -4; ix <= 4; ix++) {\n"
    "        for (int iy = -4; iy <= 4; iy++) {\n"
    "            vec2 off = vec2(float(ix), float(iy)) * ps * r * 0.5;\n"
    "            float w = 1.0 / (1.0 + float(ix*ix + iy*iy));\n"
    "            c += texture(u_scene, uv + off).rgb * w;\n"
    "            tw += w;\n"
    "        }\n"
    "    }\n"
    "    c /= tw;\n"
    "    float ca = r * 0.0005;\n"
    "    c.r = mix(c.r, texture(u_scene, uv + vec2(ca, 0.0)).r, 0.15);\n"
    "    c.b = mix(c.b, texture(u_scene, uv - vec2(ca, 0.0)).b, 0.15);\n"
    "    vec3 glass = mix(c, u_tint.rgb, u_tint.a);\n"
    "    frag_color = vec4(glass, 0.88);\n"
    "}\n";

/* ============================================================
 * Shader compilation
 * ============================================================ */

static GLuint compile_shader(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, NULL, log);
        VGP_LOG_ERROR(TAG, "shader compile: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_program(GLuint vs, GLuint fs)
{
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(p, 512, NULL, log);
        VGP_LOG_ERROR(TAG, "program link: %s", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

/* ============================================================
 * FBO lifecycle
 * ============================================================ */

int vgp_fbo_init(vgp_gpu_state_t *gs, uint32_t width, uint32_t height)
{
    /* Compile blur shader */
    /* Try simple shader first (most compatible), fall back to complex */
    VGP_LOG_INFO(TAG, "compiling glass blur shader...");
    GLuint vs = compile_shader(GL_VERTEX_SHADER, blur_vert_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, blur_frag_simple);
    if (!vs || !fs) {
        VGP_LOG_WARN(TAG, "simple glass shader failed, trying complex...");
        if (fs) glDeleteShader(fs);
        fs = compile_shader(GL_FRAGMENT_SHADER, blur_frag_src);
    }
    if (!vs || !fs) {
        VGP_LOG_ERROR(TAG, "all glass shaders failed (vs=%u fs=%u)", vs, fs);
        gs->blur_program = 0;
    } else {
        gs->blur_program = link_program(vs, fs);
        glDeleteShader(vs); glDeleteShader(fs);
        if (!gs->blur_program)
            VGP_LOG_ERROR(TAG, "glass shader link failed");
        else
            VGP_LOG_INFO(TAG, "glass shader ready (program=%u)", gs->blur_program);
    }

    /* Fullscreen quad VAO */
    float quad[] = {
        /* pos x,y   uv x,y */
        -1, -1,  0, 0,
         1, -1,  1, 0,
         1,  1,  1, 1,
        -1, -1,  0, 0,
         1,  1,  1, 1,
        -1,  1,  0, 1,
    };
    glGenVertexArrays(1, &gs->blur_vao);
    glGenBuffers(1, &gs->blur_vbo);
    glBindVertexArray(gs->blur_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gs->blur_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 16, (void*)8);
    glBindVertexArray(0);

    /* Create FBO texture */
    return vgp_fbo_resize(gs, width, height);
}

int vgp_fbo_resize(vgp_gpu_state_t *gs, uint32_t width, uint32_t height)
{
    if (gs->fbo_width == width && gs->fbo_height == height && gs->fbo_initialized)
        return 0;

    /* Delete old */
    if (gs->fbo) glDeleteFramebuffers(1, &gs->fbo);
    if (gs->fbo_texture) glDeleteTextures(1, &gs->fbo_texture);
    if (gs->fbo_depth) glDeleteRenderbuffers(1, &gs->fbo_depth);

    /* Scene texture */
    glGenTextures(1, &gs->fbo_texture);
    glBindTexture(GL_TEXTURE_2D, gs->fbo_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)width, (GLsizei)height,
                  0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    /* Depth/stencil */
    glGenRenderbuffers(1, &gs->fbo_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, gs->fbo_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)width, (GLsizei)height);

    /* FBO */
    glGenFramebuffers(1, &gs->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gs->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gs->fbo_texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gs->fbo_depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        VGP_LOG_ERROR(TAG, "FBO incomplete: 0x%x", status);
        return -1;
    }

    gs->fbo_width = width;
    gs->fbo_height = height;
    gs->fbo_initialized = true;
    VGP_LOG_INFO(TAG, "FBO initialized %ux%u", width, height);
    return 0;
}

void vgp_fbo_capture(vgp_gpu_state_t *gs, uint32_t width, uint32_t height)
{
    if (!gs->fbo_initialized) return;

    /* Copy default framebuffer content into our texture.
     * Use glCopyTexSubImage2D instead of glBlitFramebuffer for
     * maximum driver compatibility (GBM/EGL surfaces). */
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, gs->fbo_texture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0,
                          (GLsizei)width, (GLsizei)height);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        VGP_LOG_WARN("fbo", "capture glCopyTexSubImage2D error: 0x%x", err);
    }
}

void vgp_fbo_draw_blur_rect(vgp_gpu_state_t *gs,
                              float x, float y, float w, float h,
                              float corner_radius,
                              float blur_radius,
                              float tint_r, float tint_g, float tint_b, float tint_a,
                              uint32_t screen_w, uint32_t screen_h)
{
    if (!gs->fbo_initialized || !gs->blur_program) return;

    glUseProgram(gs->blur_program);

    /* Bind scene texture */
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gs->fbo_texture);
    glUniform1i(glGetUniformLocation(gs->blur_program, "u_scene"), 0);

    /* Uniforms (only set ones that exist in the active shader variant) */
    GLint loc;
    loc = glGetUniformLocation(gs->blur_program, "u_resolution");
    if (loc >= 0) glUniform2f(loc, (float)screen_w, (float)screen_h);
    loc = glGetUniformLocation(gs->blur_program, "u_blur_radius");
    if (loc >= 0) glUniform1f(loc, blur_radius);
    loc = glGetUniformLocation(gs->blur_program, "u_tint");
    if (loc >= 0) glUniform4f(loc, tint_r, tint_g, tint_b, tint_a);
    loc = glGetUniformLocation(gs->blur_program, "u_rect");
    if (loc >= 0) glUniform4f(loc, x, y, w, h);
    loc = glGetUniformLocation(gs->blur_program, "u_corner_radius");
    if (loc >= 0) glUniform1f(loc, corner_radius);

    /* NDC position of the glass rectangle.
     * OpenGL NDC: (-1,-1) = bottom-left, (1,1) = top-right.
     * Screen coords: (0,0) = top-left, (w,h) = bottom-right.
     * So screen Y must be flipped for NDC. */
    float ndc_x0 = (x / (float)screen_w) * 2.0f - 1.0f;
    float ndc_x1 = ((x + w) / (float)screen_w) * 2.0f - 1.0f;
    float ndc_y0 = 1.0f - ((y + h) / (float)screen_h) * 2.0f; /* bottom of rect in NDC */
    float ndc_y1 = 1.0f - (y / (float)screen_h) * 2.0f;       /* top of rect in NDC */

    /* UV coordinates to sample the captured texture.
     * The texture was captured with glCopyTexSubImage2D from the GL
     * framebuffer, so texture V=0 = bottom of screen, V=1 = top.
     * Screen y=0 (top) maps to texture v = 1 - 0 = 1.
     * Screen y=h (bottom) maps to texture v = 1 - 1 = 0. */
    float uv_x0 = x / (float)screen_w;
    float uv_x1 = (x + w) / (float)screen_w;
    float uv_y0 = 1.0f - (y + h) / (float)screen_h; /* bottom of rect in tex */
    float uv_y1 = 1.0f - y / (float)screen_h;       /* top of rect in tex */

    float quad[] = {
        ndc_x0, ndc_y0,  uv_x0, uv_y0,  /* bottom-left */
        ndc_x1, ndc_y0,  uv_x1, uv_y0,  /* bottom-right */
        ndc_x1, ndc_y1,  uv_x1, uv_y1,  /* top-right */
        ndc_x0, ndc_y0,  uv_x0, uv_y0,  /* bottom-left */
        ndc_x1, ndc_y1,  uv_x1, uv_y1,  /* top-right */
        ndc_x0, ndc_y1,  uv_x0, uv_y1,  /* top-left */
    };

    glBindVertexArray(gs->blur_vao);
    glBindBuffer(GL_ARRAY_BUFFER, gs->blur_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glUseProgram(0);
}

void vgp_fbo_destroy(vgp_gpu_state_t *gs)
{
    if (gs->blur_program) glDeleteProgram(gs->blur_program);
    if (gs->blur_vao) glDeleteVertexArrays(1, &gs->blur_vao);
    if (gs->blur_vbo) glDeleteBuffers(1, &gs->blur_vbo);
    if (gs->fbo) glDeleteFramebuffers(1, &gs->fbo);
    if (gs->fbo_texture) glDeleteTextures(1, &gs->fbo_texture);
    if (gs->fbo_depth) glDeleteRenderbuffers(1, &gs->fbo_depth);
}

#endif /* VGP_HAS_GPU_BACKEND */
