/* SPDX-License-Identifier: MIT */
#ifdef VGP_HAS_GPU_BACKEND

#include "shader_loader.h"
#include "vgp/log.h"

#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "shader"

static const char *vert_src =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location=0) in vec2 a_pos;\n"
    "uniform vec4 u_rect;\n"
    "uniform vec2 u_resolution;\n"
    "out vec2 v_uv;\n"
    "out vec2 v_pixel;\n"
    "void main() {\n"
    "    v_uv = a_pos;\n"
    "    v_pixel = u_rect.xy + a_pos * u_rect.zw;\n"
    "    vec2 ndc = (v_pixel / u_resolution) * 2.0 - 1.0;\n"
    "    ndc.y = -ndc.y;\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";

static const char *frag_preamble =
    "#version 300 es\n"
    "precision highp float;\n"
    "uniform float u_time;\n"
    "uniform vec2  u_resolution;\n"
    "uniform vec4  u_rect;\n"
    "uniform vec4  u_color;\n"
    "uniform vec4  u_accent;\n"
    "uniform vec2  u_mouse;\n"
    "uniform vec4  u_windows[8];\n"
    "uniform int   u_window_count;\n"
    "in vec2 v_uv;\n"
    "in vec2 v_pixel;\n"
    "out vec4 frag_color;\n"
    "\n";

static const char *frag_epilogue =
    "\nvoid main() {\n"
    "    vec4 color = vec4(0.0);\n"
    "    effect(color, v_uv, v_pixel);\n"
    "    frag_color = color;\n"
    "}\n";

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        VGP_LOG_ERROR(TAG, "compile: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

static GLuint link_prog(const char *full_frag)
{
    GLuint vs = compile(GL_VERTEX_SHADER, vert_src);
    GLuint fs = compile(GL_FRAGMENT_SHADER, full_frag);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), NULL, log);
        VGP_LOG_ERROR(TAG, "link: %s", log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

int vgp_shader_mgr_init(vgp_shader_mgr_t *mgr)
{
    memset(mgr, 0, sizeof(*mgr));

    float quad[] = { 0,0, 1,0, 0,1, 1,0, 1,1, 0,1 };
    glGenVertexArrays(1, &mgr->quad_vao);
    glGenBuffers(1, &mgr->quad_vbo);
    glBindVertexArray(mgr->quad_vao);
    glBindBuffer(GL_ARRAY_BUFFER, mgr->quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, NULL);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    mgr->initialized = true;
    VGP_LOG_INFO(TAG, "shader manager initialized");
    return 0;
}

void vgp_shader_mgr_destroy(vgp_shader_mgr_t *mgr)
{
    for (int i = 0; i < mgr->count; i++) {
        if (mgr->shaders[i].program)
            glDeleteProgram(mgr->shaders[i].program);
    }
    if (mgr->quad_vao) glDeleteVertexArrays(1, &mgr->quad_vao);
    if (mgr->quad_vbo) glDeleteBuffers(1, &mgr->quad_vbo);
    mgr->initialized = false;
}

int vgp_shader_load(vgp_shader_mgr_t *mgr, const char *frag_path)
{
    if (mgr->count >= VGP_MAX_SHADERS) return -1;

    char *user_src = read_file(frag_path);
    if (!user_src) {
        VGP_LOG_ERROR(TAG, "cannot read: %s", frag_path);
        return -1;
    }

    /* Build: preamble + user code + epilogue */
    size_t total = strlen(frag_preamble) + strlen(user_src) + strlen(frag_epilogue) + 1;
    char *full = malloc(total);
    if (!full) { free(user_src); return -1; }
    snprintf(full, total, "%s%s%s", frag_preamble, user_src, frag_epilogue);
    free(user_src);

    GLuint prog = link_prog(full);
    free(full);
    if (!prog) return -1;

    int idx = mgr->count++;
    vgp_shader_t *s = &mgr->shaders[idx];
    s->program = prog;
    s->u_time = glGetUniformLocation(prog, "u_time");
    s->u_resolution = glGetUniformLocation(prog, "u_resolution");
    s->u_rect = glGetUniformLocation(prog, "u_rect");
    s->u_color = glGetUniformLocation(prog, "u_color");
    s->u_accent = glGetUniformLocation(prog, "u_accent");
    s->u_mouse = glGetUniformLocation(prog, "u_mouse");
    s->u_windows = glGetUniformLocation(prog, "u_windows");
    s->u_window_count = glGetUniformLocation(prog, "u_window_count");
    snprintf(s->path, sizeof(s->path), "%s", frag_path);
    s->loaded = true;

    VGP_LOG_INFO(TAG, "loaded shader %d: %s", idx, frag_path);
    return idx;
}

void vgp_shader_render(vgp_shader_mgr_t *mgr, int shader_idx,
                        float x, float y, float w, float h,
                        float screen_w, float screen_h,
                        float r, float g, float b, float a,
                        float ar, float ag, float ab, float aa,
                        float mouse_x, float mouse_y,
                        const vgp_shader_windows_t *windows)
{
    if (shader_idx < 0 || shader_idx >= mgr->count) return;
    vgp_shader_t *s = &mgr->shaders[shader_idx];
    if (!s->loaded) return;

    glUseProgram(s->program);
    glUniform1f(s->u_time, mgr->time);
    glUniform2f(s->u_resolution, screen_w, screen_h);
    glUniform4f(s->u_rect, x, y, w, h);
    glUniform4f(s->u_color, r, g, b, a);
    glUniform4f(s->u_accent, ar, ag, ab, aa);
    glUniform2f(s->u_mouse, mouse_x, mouse_y);

    if (windows && s->u_windows >= 0) {
        glUniform4fv(s->u_windows, VGP_SHADER_MAX_WINDOWS, windows->rects);
        glUniform1i(s->u_window_count, windows->count);
    }

    glBindVertexArray(mgr->quad_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glUseProgram(0);
}

void vgp_shader_mgr_tick(vgp_shader_mgr_t *mgr, float dt)
{
    mgr->time += dt;
}

#endif /* VGP_HAS_GPU_BACKEND */