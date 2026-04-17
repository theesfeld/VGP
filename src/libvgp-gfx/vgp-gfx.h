/* SPDX-License-Identifier: MIT */
#ifndef VGP_GFX_H
#define VGP_GFX_H

/* VGP Graphical UI Toolkit
 * Immediate-mode vector graphics API for VGP native applications.
 * Builds compact draw command streams sent to the server for GPU rendering.
 * All coordinates are window-local floats. Theme colors auto-applied.
 */

#include "vgp/vgp.h"
#include "vgp/protocol.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * Color type
 * ============================================================ */

typedef struct vgfx_color {
    float r, g, b, a;
} vgfx_color_t;

static inline vgfx_color_t vgfx_rgba(float r, float g, float b, float a)
{ return (vgfx_color_t){r, g, b, a}; }

static inline vgfx_color_t vgfx_rgb(float r, float g, float b)
{ return (vgfx_color_t){r, g, b, 1.0f}; }

static inline vgfx_color_t vgfx_hex(uint32_t hex)
{ return (vgfx_color_t){((hex>>16)&0xFF)/255.0f, ((hex>>8)&0xFF)/255.0f, (hex&0xFF)/255.0f, 1.0f}; }

static inline vgfx_color_t vgfx_alpha(vgfx_color_t c, float a)
{ return (vgfx_color_t){c.r, c.g, c.b, a}; }

/* ============================================================
 * Theme
 * ============================================================ */

typedef struct vgfx_theme {
    float colors[16][4];       /* 16 semantic colors (RGBA) */
    float font_size;
    float font_size_small;
    float font_size_large;
    float corner_radius;
    float padding;
    float spacing;
    float border_width;
    float scrollbar_width;
    float button_height;
    float input_height;
    float checkbox_size;
    float slider_height;
    float char_advances[95];   /* ASCII advance widths at font_size */
    bool  received;            /* true once server sends theme info */
} vgfx_theme_t;

/* ============================================================
 * Context
 * ============================================================ */

#define VGFX_CMD_BUF_INIT 8192

typedef struct vgfx_ctx {
    vgp_connection_t *conn;
    uint32_t          window_id;

    /* Draw command buffer */
    uint8_t          *cmd_buf;
    size_t            cmd_len;
    size_t            cmd_cap;
    uint32_t          cmd_count;

    /* Theme */
    vgfx_theme_t      theme;

    /* Window geometry */
    float             width, height;

    /* Input state */
    float             mouse_x, mouse_y;
    bool              mouse_pressed;
    bool              mouse_clicked;
    float             scroll_dy;

    uint32_t          last_keysym;
    uint32_t          last_mods;
    char              last_utf8[8];
    bool              key_pressed;

    /* Immediate-mode widget state */
    uint32_t          hot_id;       /* hovered widget */
    uint32_t          active_id;    /* interacting widget */
    uint32_t          focus_id;     /* keyboard focus */
    uint32_t          next_id;      /* auto-increment per frame */

    /* Scroll offset (for scrollable pages) */
    int               scroll_offset;

    bool              running;
    bool              dirty;
} vgfx_ctx_t;

/* ============================================================
 * Lifecycle
 * ============================================================ */

int  vgfx_init(vgfx_ctx_t *ctx, const char *title, int width, int height, uint32_t flags);
void vgfx_destroy(vgfx_ctx_t *ctx);
void vgfx_begin_frame(vgfx_ctx_t *ctx);
void vgfx_end_frame(vgfx_ctx_t *ctx);
int  vgfx_poll(vgfx_ctx_t *ctx, int timeout_ms);
void vgfx_run(vgfx_ctx_t *ctx, void (*render)(vgfx_ctx_t *ctx));

/* Continuous animation loop: renders every frame at ~60fps.
 * sample() is called every sample_interval_ms for data collection. */
void vgfx_run_animated(vgfx_ctx_t *ctx,
                         void (*render)(vgfx_ctx_t *ctx),
                         void (*sample)(void),
                         int sample_interval_ms);

/* ============================================================
 * Drawing primitives
 * ============================================================ */

void vgfx_clear(vgfx_ctx_t *ctx, vgfx_color_t color);
void vgfx_rect(vgfx_ctx_t *ctx, float x, float y, float w, float h, vgfx_color_t c);
void vgfx_rounded_rect(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                         float radius, vgfx_color_t c);
void vgfx_circle(vgfx_ctx_t *ctx, float cx, float cy, float r, vgfx_color_t c);
void vgfx_line(vgfx_ctx_t *ctx, float x1, float y1, float x2, float y2,
                float width, vgfx_color_t c);
void vgfx_text(vgfx_ctx_t *ctx, const char *text, float x, float y,
                float size, vgfx_color_t c);
void vgfx_text_bold(vgfx_ctx_t *ctx, const char *text, float x, float y,
                      float size, vgfx_color_t c);
void vgfx_rect_outline(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                         float line_w, vgfx_color_t c);
void vgfx_rounded_rect_outline(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                                  float radius, float line_w, vgfx_color_t c);
void vgfx_gradient_rect(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                          vgfx_color_t top, vgfx_color_t bottom);
void vgfx_push_clip(vgfx_ctx_t *ctx, float x, float y, float w, float h);
void vgfx_pop_clip(vgfx_ctx_t *ctx);

/* ============================================================
 * Theme access
 * ============================================================ */

vgfx_color_t vgfx_theme_color(vgfx_ctx_t *ctx, int slot);
float        vgfx_text_width(vgfx_ctx_t *ctx, const char *text, int len, float size);
float        vgfx_text_height(vgfx_ctx_t *ctx, float size);

/* ============================================================
 * Widgets (immediate mode)
 * ============================================================ */

void vgfx_label(vgfx_ctx_t *ctx, float x, float y, const char *text);
void vgfx_label_colored(vgfx_ctx_t *ctx, float x, float y, const char *text, vgfx_color_t c);
void vgfx_heading(vgfx_ctx_t *ctx, float x, float y, const char *text);
void vgfx_section(vgfx_ctx_t *ctx, float x, float y, float w, const char *title);
void vgfx_separator(vgfx_ctx_t *ctx, float x, float y, float w);

bool vgfx_button(vgfx_ctx_t *ctx, float x, float y, float w, float h, const char *label);
bool vgfx_checkbox(vgfx_ctx_t *ctx, float x, float y, const char *label, bool *value);
bool vgfx_slider(vgfx_ctx_t *ctx, float x, float y, float w,
                   float *value, float min, float max, const char *fmt);
bool vgfx_dropdown(vgfx_ctx_t *ctx, float x, float y, float w,
                     const char **items, int count, int *selected, bool *open);
bool vgfx_text_input(vgfx_ctx_t *ctx, float x, float y, float w,
                       char *buffer, int buf_size);
bool vgfx_list_item(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                      const char *text, bool selected);
void vgfx_progress(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                     float value, vgfx_color_t c);
void vgfx_tooltip(vgfx_ctx_t *ctx, float area_x, float area_y, float area_w, float area_h,
                    const char *text);
bool vgfx_scrollbar(vgfx_ctx_t *ctx, float x, float y, float h,
                      int visible, int total, int *offset);

#endif /* VGP_GFX_H */