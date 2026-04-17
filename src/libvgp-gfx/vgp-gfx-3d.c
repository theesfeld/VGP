/* SPDX-License-Identifier: MIT */
/* VGP 3D Math + Wireframe Rendering
 * All 3D projection is client-side. Results are 2D LINE commands. */

#include "vgp-gfx-3d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================
 * Matrix operations
 * ============================================================ */

/* Column-major indexing: m[col*4 + row] */
#define M(mat, r, c) ((mat).m[(c)*4 + (r)])

vgfx_mat4_t vgfx_mat4_identity(void)
{
    vgfx_mat4_t m;
    memset(&m, 0, sizeof(m));
    M(m,0,0) = M(m,1,1) = M(m,2,2) = M(m,3,3) = 1.0f;
    return m;
}

vgfx_mat4_t vgfx_mat4_multiply(vgfx_mat4_t a, vgfx_mat4_t b)
{
    vgfx_mat4_t r;
    for (int c = 0; c < 4; c++)
        for (int row = 0; row < 4; row++) {
            float sum = 0;
            for (int k = 0; k < 4; k++)
                sum += M(a, row, k) * M(b, k, c);
            M(r, row, c) = sum;
        }
    return r;
}

vgfx_mat4_t vgfx_mat4_rotate_x(float rad)
{
    vgfx_mat4_t m = vgfx_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    M(m,1,1) = c;  M(m,1,2) = -s;
    M(m,2,1) = s;  M(m,2,2) = c;
    return m;
}

vgfx_mat4_t vgfx_mat4_rotate_y(float rad)
{
    vgfx_mat4_t m = vgfx_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    M(m,0,0) = c;  M(m,0,2) = s;
    M(m,2,0) = -s; M(m,2,2) = c;
    return m;
}

vgfx_mat4_t vgfx_mat4_rotate_z(float rad)
{
    vgfx_mat4_t m = vgfx_mat4_identity();
    float c = cosf(rad), s = sinf(rad);
    M(m,0,0) = c;  M(m,0,1) = -s;
    M(m,1,0) = s;  M(m,1,1) = c;
    return m;
}

vgfx_mat4_t vgfx_mat4_translate(float x, float y, float z)
{
    vgfx_mat4_t m = vgfx_mat4_identity();
    M(m,0,3) = x; M(m,1,3) = y; M(m,2,3) = z;
    return m;
}

vgfx_mat4_t vgfx_mat4_scale(float sx, float sy, float sz)
{
    vgfx_mat4_t m = vgfx_mat4_identity();
    M(m,0,0) = sx; M(m,1,1) = sy; M(m,2,2) = sz;
    return m;
}

vgfx_mat4_t vgfx_mat4_perspective(float fov_rad, float aspect, float near, float far)
{
    vgfx_mat4_t m;
    memset(&m, 0, sizeof(m));
    float f = 1.0f / tanf(fov_rad * 0.5f);
    M(m,0,0) = f / aspect;
    M(m,1,1) = f;
    M(m,2,2) = (far + near) / (near - far);
    M(m,2,3) = (2.0f * far * near) / (near - far);
    M(m,3,2) = -1.0f;
    return m;
}

vgfx_vec4_t vgfx_mat4_mul_vec4(vgfx_mat4_t m, vgfx_vec4_t v)
{
    vgfx_vec4_t r;
    r.x = M(m,0,0)*v.x + M(m,0,1)*v.y + M(m,0,2)*v.z + M(m,0,3)*v.w;
    r.y = M(m,1,0)*v.x + M(m,1,1)*v.y + M(m,1,2)*v.z + M(m,1,3)*v.w;
    r.z = M(m,2,0)*v.x + M(m,2,1)*v.y + M(m,2,2)*v.z + M(m,2,3)*v.w;
    r.w = M(m,3,0)*v.x + M(m,3,1)*v.y + M(m,3,2)*v.z + M(m,3,3)*v.w;
    return r;
}

vgfx_vec3_t vgfx_mat4_mul_point(vgfx_mat4_t m, vgfx_vec3_t p)
{
    vgfx_vec4_t v = {p.x, p.y, p.z, 1.0f};
    vgfx_vec4_t r = vgfx_mat4_mul_vec4(m, v);
    if (r.w != 0.0f) { r.x /= r.w; r.y /= r.w; r.z /= r.w; }
    return (vgfx_vec3_t){r.x, r.y, r.z};
}

/* ============================================================
 * 3D Projection
 * ============================================================ */

void vgfx_3d_init(vgfx_3d_ctx_t *ctx3d, float cx, float cy,
                    float scale, float fov_degrees)
{
    memset(ctx3d, 0, sizeof(*ctx3d));
    ctx3d->model = vgfx_mat4_identity();
    ctx3d->view = vgfx_mat4_translate(0, 0, -3.0f); /* camera back */
    ctx3d->projection = vgfx_mat4_perspective(
        fov_degrees * (float)M_PI / 180.0f, 1.0f, 0.1f, 100.0f);
    ctx3d->screen_cx = cx;
    ctx3d->screen_cy = cy;
    ctx3d->screen_scale = scale;
    ctx3d->mvp = vgfx_mat4_multiply(ctx3d->projection,
                  vgfx_mat4_multiply(ctx3d->view, ctx3d->model));
}

void vgfx_3d_set_model(vgfx_3d_ctx_t *ctx3d, vgfx_mat4_t model)
{
    ctx3d->model = model;
    ctx3d->mvp = vgfx_mat4_multiply(ctx3d->projection,
                  vgfx_mat4_multiply(ctx3d->view, ctx3d->model));
}

bool vgfx_project(vgfx_3d_ctx_t *ctx3d, vgfx_vec3_t p, float *sx, float *sy)
{
    vgfx_vec4_t v = {p.x, p.y, p.z, 1.0f};
    vgfx_vec4_t clip = vgfx_mat4_mul_vec4(ctx3d->mvp, v);

    /* Behind camera? */
    if (clip.w <= 0.001f) return false;

    /* Perspective divide -> NDC */
    float nx = clip.x / clip.w;
    float ny = clip.y / clip.w;

    /* NDC to screen (Y is inverted for screen coords) */
    *sx = ctx3d->screen_cx + nx * ctx3d->screen_scale;
    *sy = ctx3d->screen_cy - ny * ctx3d->screen_scale;
    return true;
}

/* ============================================================
 * Mesh generation
 * ============================================================ */

static vgfx_mesh_t *mesh_alloc(int verts, int edges)
{
    vgfx_mesh_t *m = calloc(1, sizeof(*m));
    if (!m) return NULL;
    m->verts = calloc((size_t)verts, sizeof(vgfx_vec3_t));
    m->edges = calloc((size_t)edges, sizeof(vgfx_edge_t));
    if (!m->verts || !m->edges) { free(m->verts); free(m->edges); free(m); return NULL; }
    m->vert_count = verts;
    m->edge_count = edges;
    return m;
}

void vgfx_mesh_destroy(vgfx_mesh_t *mesh)
{
    if (!mesh) return;
    free(mesh->verts);
    free(mesh->edges);
    free(mesh);
}

vgfx_mesh_t *vgfx_mesh_sphere(int lat_divs, int lon_divs, float radius)
{
    /* Vertices: 2 poles + (lat_divs-1) * lon_divs grid points */
    int n_verts = 2 + (lat_divs - 1) * lon_divs;
    /* Edges: lat rings + lon lines */
    int n_lat_edges = (lat_divs - 1) * lon_divs;
    int n_lon_edges = lat_divs * lon_divs;
    int n_edges = n_lat_edges + n_lon_edges;

    vgfx_mesh_t *m = mesh_alloc(n_verts, n_edges);
    if (!m) return NULL;

    /* North pole = vertex 0 */
    m->verts[0] = (vgfx_vec3_t){0, radius, 0};
    /* South pole = vertex 1 */
    m->verts[1] = (vgfx_vec3_t){0, -radius, 0};

    /* Grid vertices */
    int vi = 2;
    for (int i = 1; i < lat_divs; i++) {
        float theta = (float)M_PI * (float)i / (float)lat_divs;
        float sin_t = sinf(theta), cos_t = cosf(theta);
        for (int j = 0; j < lon_divs; j++) {
            float phi = 2.0f * (float)M_PI * (float)j / (float)lon_divs;
            m->verts[vi++] = (vgfx_vec3_t){
                radius * sin_t * cosf(phi),
                radius * cos_t,
                radius * sin_t * sinf(phi)
            };
        }
    }

    /* Edges: latitude rings */
    int ei = 0;
    for (int i = 0; i < lat_divs - 1; i++) {
        for (int j = 0; j < lon_divs; j++) {
            int v0 = 2 + i * lon_divs + j;
            int v1 = 2 + i * lon_divs + (j + 1) % lon_divs;
            m->edges[ei++] = (vgfx_edge_t){(uint16_t)v0, (uint16_t)v1};
        }
    }

    /* Edges: longitude lines */
    for (int j = 0; j < lon_divs; j++) {
        /* North pole to first ring */
        m->edges[ei++] = (vgfx_edge_t){0, (uint16_t)(2 + j)};
        /* Between rings */
        for (int i = 0; i < lat_divs - 2; i++) {
            int v0 = 2 + i * lon_divs + j;
            int v1 = 2 + (i + 1) * lon_divs + j;
            m->edges[ei++] = (vgfx_edge_t){(uint16_t)v0, (uint16_t)v1};
        }
        /* Last ring to south pole */
        m->edges[ei++] = (vgfx_edge_t){(uint16_t)(2 + (lat_divs - 2) * lon_divs + j), 1};
    }

    m->edge_count = ei;
    return m;
}

vgfx_mesh_t *vgfx_mesh_cube(float h)
{
    vgfx_mesh_t *m = mesh_alloc(8, 12);
    if (!m) return NULL;
    /* 8 corners */
    float v[][3] = {{-h,-h,-h},{h,-h,-h},{h,h,-h},{-h,h,-h},
                     {-h,-h,h},{h,-h,h},{h,h,h},{-h,h,h}};
    for (int i = 0; i < 8; i++) m->verts[i] = (vgfx_vec3_t){v[i][0], v[i][1], v[i][2]};
    /* 12 edges */
    int e[][2] = {{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
                   {0,4},{1,5},{2,6},{3,7}};
    for (int i = 0; i < 12; i++) m->edges[i] = (vgfx_edge_t){(uint16_t)e[i][0], (uint16_t)e[i][1]};
    return m;
}

vgfx_mesh_t *vgfx_mesh_grid(int divs, float extent)
{
    int lines = (divs + 1) * 2; /* horizontal + vertical */
    int n_verts = lines * 2;
    int n_edges = lines;
    vgfx_mesh_t *m = mesh_alloc(n_verts, n_edges);
    if (!m) return NULL;

    int vi = 0, ei = 0;
    float step = (2.0f * extent) / (float)divs;
    for (int i = 0; i <= divs; i++) {
        float t = -extent + (float)i * step;
        /* Horizontal line (along X) */
        m->verts[vi] = (vgfx_vec3_t){-extent, 0, t};
        m->verts[vi+1] = (vgfx_vec3_t){extent, 0, t};
        m->edges[ei++] = (vgfx_edge_t){(uint16_t)vi, (uint16_t)(vi+1)};
        vi += 2;
        /* Vertical line (along Z) */
        m->verts[vi] = (vgfx_vec3_t){t, 0, -extent};
        m->verts[vi+1] = (vgfx_vec3_t){t, 0, extent};
        m->edges[ei++] = (vgfx_edge_t){(uint16_t)vi, (uint16_t)(vi+1)};
        vi += 2;
    }
    m->vert_count = vi;
    m->edge_count = ei;
    return m;
}

vgfx_mesh_t *vgfx_mesh_cylinder(int segments, float radius, float height)
{
    int n_verts = segments * 2;
    int n_edges = segments * 3; /* top ring + bottom ring + verticals */
    vgfx_mesh_t *m = mesh_alloc(n_verts, n_edges);
    if (!m) return NULL;

    float hy = height * 0.5f;
    for (int i = 0; i < segments; i++) {
        float a = 2.0f * (float)M_PI * (float)i / (float)segments;
        float x = radius * cosf(a), z = radius * sinf(a);
        m->verts[i] = (vgfx_vec3_t){x, hy, z};
        m->verts[segments + i] = (vgfx_vec3_t){x, -hy, z};
    }
    int ei = 0;
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        m->edges[ei++] = (vgfx_edge_t){(uint16_t)i, (uint16_t)next}; /* top ring */
        m->edges[ei++] = (vgfx_edge_t){(uint16_t)(segments+i), (uint16_t)(segments+next)}; /* bottom ring */
        m->edges[ei++] = (vgfx_edge_t){(uint16_t)i, (uint16_t)(segments+i)}; /* vertical */
    }
    m->edge_count = ei;
    return m;
}

vgfx_mesh_t *vgfx_mesh_ring(int segments, float radius)
{
    vgfx_mesh_t *m = mesh_alloc(segments, segments);
    if (!m) return NULL;
    for (int i = 0; i < segments; i++) {
        float a = 2.0f * (float)M_PI * (float)i / (float)segments;
        m->verts[i] = (vgfx_vec3_t){radius * cosf(a), radius * sinf(a), 0};
    }
    for (int i = 0; i < segments; i++)
        m->edges[i] = (vgfx_edge_t){(uint16_t)i, (uint16_t)((i+1) % segments)};
    return m;
}

/* ============================================================
 * 3D wireframe drawing
 * ============================================================ */

int vgfx_draw_mesh(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d,
                     const vgfx_mesh_t *mesh,
                     float line_width, vgfx_color_t color)
{
    int drawn = 0;
    for (int i = 0; i < mesh->edge_count; i++) {
        vgfx_vec3_t va = mesh->verts[mesh->edges[i].a];
        vgfx_vec3_t vb = mesh->verts[mesh->edges[i].b];
        float sx0, sy0, sx1, sy1;
        if (vgfx_project(ctx3d, va, &sx0, &sy0) &&
            vgfx_project(ctx3d, vb, &sx1, &sy1)) {
            vgfx_line(ctx, sx0, sy0, sx1, sy1, line_width, color);
            drawn++;
        }
    }
    return drawn;
}

int vgfx_draw_mesh_colored(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d,
                             const vgfx_mesh_t *mesh,
                             float line_width,
                             const vgfx_color_t *edge_colors)
{
    int drawn = 0;
    for (int i = 0; i < mesh->edge_count; i++) {
        vgfx_vec3_t va = mesh->verts[mesh->edges[i].a];
        vgfx_vec3_t vb = mesh->verts[mesh->edges[i].b];
        float sx0, sy0, sx1, sy1;
        if (vgfx_project(ctx3d, va, &sx0, &sy0) &&
            vgfx_project(ctx3d, vb, &sx1, &sy1)) {
            vgfx_line(ctx, sx0, sy0, sx1, sy1, line_width, edge_colors[i]);
            drawn++;
        }
    }
    return drawn;
}

void vgfx_draw_line_3d(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d,
                         vgfx_vec3_t a, vgfx_vec3_t b,
                         float width, vgfx_color_t color)
{
    float sx0, sy0, sx1, sy1;
    if (vgfx_project(ctx3d, a, &sx0, &sy0) &&
        vgfx_project(ctx3d, b, &sx1, &sy1))
        vgfx_line(ctx, sx0, sy0, sx1, sy1, width, color);
}

void vgfx_draw_axes(vgfx_ctx_t *ctx, vgfx_3d_ctx_t *ctx3d, float length)
{
    vgfx_vec3_t o = {0,0,0};
    vgfx_draw_line_3d(ctx, ctx3d, o, (vgfx_vec3_t){length,0,0}, 1.5f, vgfx_rgba(1,0,0,1)); /* X = red */
    vgfx_draw_line_3d(ctx, ctx3d, o, (vgfx_vec3_t){0,length,0}, 1.5f, vgfx_rgba(0,1,0,1)); /* Y = green */
    vgfx_draw_line_3d(ctx, ctx3d, o, (vgfx_vec3_t){0,0,length}, 1.5f, vgfx_rgba(0,0,1,1)); /* Z = blue */
}