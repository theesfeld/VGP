#ifndef VGP_SHADER_LOADER_H
#define VGP_SHADER_LOADER_H

#ifdef VGP_HAS_GPU_BACKEND

#include <stdbool.h>
#include <stdint.h>

#define VGP_MAX_SHADERS 32

typedef struct vgp_shader {
    unsigned int program;
    int          u_time;
    int          u_resolution;
    int          u_rect;
    int          u_color;
    int          u_accent;
    int          u_mouse;
    int          u_windows;     /* vec4 array of window rects */
    int          u_window_count;
    char         path[256];
    bool         loaded;
} vgp_shader_t;

typedef struct vgp_shader_mgr {
    vgp_shader_t shaders[VGP_MAX_SHADERS];
    int          count;
    unsigned int quad_vao;
    unsigned int quad_vbo;
    float        time;       /* seconds since start */
    bool         initialized;
} vgp_shader_mgr_t;

/* Initialize the shader manager (needs active GL context) */
int  vgp_shader_mgr_init(vgp_shader_mgr_t *mgr);
void vgp_shader_mgr_destroy(vgp_shader_mgr_t *mgr);

/* Load a shader from a .frag file. Returns shader index or -1. */
int  vgp_shader_load(vgp_shader_mgr_t *mgr, const char *frag_path);

/* Window rects for shadow casting (max 8 windows) */
#define VGP_SHADER_MAX_WINDOWS 8

typedef struct vgp_shader_windows {
    float rects[VGP_SHADER_MAX_WINDOWS * 4]; /* x,y,w,h packed */
    int   count;
} vgp_shader_windows_t;

/* Render a shader into a screen rectangle. */
void vgp_shader_render(vgp_shader_mgr_t *mgr, int shader_idx,
                        float x, float y, float w, float h,
                        float screen_w, float screen_h,
                        float r, float g, float b, float a,
                        float ar, float ag, float ab, float aa,
                        float mouse_x, float mouse_y,
                        const vgp_shader_windows_t *windows);

/* Update time (call once per frame) */
void vgp_shader_mgr_tick(vgp_shader_mgr_t *mgr, float dt);

#endif /* VGP_HAS_GPU_BACKEND */
#endif /* VGP_SHADER_LOADER_H */
