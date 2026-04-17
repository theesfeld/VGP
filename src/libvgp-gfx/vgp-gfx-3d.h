/* SPDX-License-Identifier: MIT */
#ifndef VGP_GFX_3D_H
#define VGP_GFX_3D_H

/* VGP 3D Math + Wireframe Rendering
 * Client-side 3D-to-2D projection. All 3D math happens here.
 * Projected results sent as regular 2D LINE draw commands.
 * The server never sees 3D -- it only renders 2D lines. */

#include "vgp-gfx.h"
#include <stdint.h>

/* ============================================================
 * Types
 * ============================================================ */

typedef struct { float x, y; } vgfx_vec2_t;
typedef struct { float x, y, z; } vgfx_vec3_t;
typedef struct { float x, y, z, w; } vgfx_vec4_t;

/* 4x4 matrix, column-major (OpenGL convention) */
typedef struct { float m[16]; } vgfx_mat4_t;

/* Wireframe edge: index pair into vertex array */
typedef struct { uint16_t a, b; } vgfx_edge_t;

/* Wireframe mesh */
typedef struct {
    vgfx_vec3_t *verts;
    int          vert_count;
    vgfx_edge_t *edges;
    int          edge_count;
} vgfx_mesh_t;

/* 3D projection context */
typedef struct {
    vgfx_mat4_t model;
    vgfx_mat4_t view;
    vgfx_mat4_t projection;
    vgfx_mat4_t mvp;         /* cached model * view * projection */
    float       screen_cx;   /* screen center X */
    float       screen_cy;   /* screen center Y */
    float       screen_scale;/* pixels per unit at z=1 */
} vgfx_3d_ctx_t;

/* ============================================================
 * Matrix operations
 * ============================================================ */

vgfx_mat4_t vgfx_mat4_identity(void);
vgfx_mat4_t vgfx_mat4_multiply(vgfx_mat4_t a, vgfx_mat4_t b);
vgfx_mat4_t vgfx_mat4_rotate_x(float radians);
vgfx_mat4_t vgfx_mat4_rotate_y(float radians);
vgfx_mat4_t vgfx_mat4_rotate_z(float radians);
vgfx_mat4_t vgfx_mat4_translate(float x, float y, float z);
vgfx_mat4_t vgfx_mat4_scale(float sx, float sy, float sz);
vgfx_mat4_t vgfx_mat4_perspective(float fov_rad, float aspect, float near, float far);

/* Transform point through matrix */
vgfx_vec4_t vgfx_mat4_mul_vec4(vgfx_mat4_t m, vgfx_vec4_t v);
vgfx_vec3_t vgfx_mat4_mul_point(vgfx_mat4_t m, vgfx_vec3_t p);

/* ============================================================
 * 3D Projection
 * ============================================================ */

/* Initialize projection context centered at (cx,cy) with given scale and FOV */
void vgfx_3d_init(vgfx_3d_ctx_t *ctx3d, float cx, float cy,
                    float scale, float fov_degrees);

/* Set model matrix and recompute MVP */
void vgfx_3d_set_model(vgfx_3d_ctx_t *ctx3d, vgfx_mat4_t model);

/* Project a 3D point to 2D screen coords. Returns false if behind camera. */
bool vgfx_project(vgfx_3d_ctx_t *ctx3d, vgfx_vec3_t p, float *sx, float *sy);

/* ============================================================
 * Mesh generation
 * ============================================================ */

vgfx_mesh_t *vgfx_mesh_sphere(int lat_divs, int lon_divs, float radius);
vgfx_mesh_t *vgfx_mesh_cube(float half_extent);
vgfx_mesh_t *vgfx_mesh_grid(int divisions, float extent);
vgfx_mesh_t *vgfx_mesh_cylinder(int segments, float radius, float height);
vgfx_mesh_t *vgfx_mesh_ring(int segments, float radius);
void          vgfx_mesh_destroy(vgfx_mesh_t *mesh);

/* ============================================================
 * 3D wireframe drawing
 * ============================================================ */

/* Draw entire mesh as wireframe. Returns number of visible edges drawn. */
int vgfx_draw_mesh(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d,
                     const vgfx_mesh_t *mesh,
                     float line_width, vgfx_color_t color);

/* Draw mesh with per-edge colors (edge_colors array must have mesh->edge_count entries) */
int vgfx_draw_mesh_colored(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d,
                             const vgfx_mesh_t *mesh,
                             float line_width,
                             const vgfx_color_t *edge_colors);

/* Draw a single 3D line segment */
void vgfx_draw_line_3d(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d,
                         vgfx_vec3_t a, vgfx_vec3_t b,
                         float width, vgfx_color_t color);

/* Draw 3D axes (RGB = XYZ) for debugging */
void vgfx_draw_axes(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d, float length);

#endif /* VGP_GFX_3D_H */