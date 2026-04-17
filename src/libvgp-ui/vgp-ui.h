/* SPDX-License-Identifier: MIT */
#ifndef VGP_UI_H
#define VGP_UI_H

#include "vgp/vgp.h"
#include "vgp/protocol.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * Cell grid UI framework for VGP native apps.
 * Immediate-mode: build the UI each frame by calling draw functions.
 * The grid is sent to the server for vector rendering.
 * ============================================================ */

#define VUI_MAX_ROWS 200
#define VUI_MAX_COLS 300

typedef struct vui_color {
    uint8_t r, g, b;
} vui_color_t;

/* Pre-defined colors */
#define VUI_WHITE   (vui_color_t){0xE0, 0xE0, 0xE0}
#define VUI_BLACK   (vui_color_t){0x10, 0x10, 0x10}
#define VUI_GRAY    (vui_color_t){0x60, 0x60, 0x70}
#define VUI_ACCENT  (vui_color_t){0x52, 0x94, 0xE2}
#define VUI_RED     (vui_color_t){0xE0, 0x60, 0x60}
#define VUI_GREEN   (vui_color_t){0x60, 0xC0, 0x60}
#define VUI_YELLOW  (vui_color_t){0xE0, 0xC0, 0x40}
#define VUI_BG      (vui_color_t){0x1A, 0x1A, 0x2E}
#define VUI_SURFACE (vui_color_t){0x22, 0x22, 0x36}
#define VUI_BORDER  (vui_color_t){0x3C, 0x3C, 0x4C}

typedef struct vui_ctx {
    vgp_connection_t *conn;
    uint32_t          window_id;

    /* Cell grid buffer */
    vgp_cell_t        cells[VUI_MAX_ROWS * VUI_MAX_COLS];
    int               rows, cols;

    /* Cursor state for keyboard navigation */
    int               cursor_row, cursor_col;
    int               selected_item;
    int               scroll_offset;

    /* Mouse state (from server events) */
    int               mouse_row, mouse_col;
    bool              mouse_pressed;
    bool              mouse_clicked; /* true on press edge */

    /* Keyboard input */
    uint32_t          last_keysym;
    uint32_t          last_mods;
    char              last_utf8[8];
    bool              key_pressed;

    /* Pixel dimensions (from configure events) */
    float             pixel_w, pixel_h;
    float             cell_px_w, cell_px_h; /* pixels per cell */

    /* App state */
    bool              running;
    bool              dirty;
} vui_ctx_t;

/* ============================================================
 * Lifecycle
 * ============================================================ */

int  vui_init(vui_ctx_t *ctx, const char *title, int width, int height);
void vui_destroy(vui_ctx_t *ctx);
void vui_begin_frame(vui_ctx_t *ctx);  /* clear grid, reset input state */
void vui_end_frame(vui_ctx_t *ctx);    /* send grid to server */
int  vui_poll(vui_ctx_t *ctx, int timeout_ms); /* process events, returns 0 on idle */
void vui_run(vui_ctx_t *ctx, void (*render)(vui_ctx_t *ctx));

/* ============================================================
 * Drawing primitives (write to cell grid)
 * ============================================================ */

void vui_clear(vui_ctx_t *ctx, vui_color_t bg);
void vui_text(vui_ctx_t *ctx, int row, int col, const char *text,
               vui_color_t fg, vui_color_t bg);
void vui_text_bold(vui_ctx_t *ctx, int row, int col, const char *text,
                    vui_color_t fg, vui_color_t bg);
void vui_hline(vui_ctx_t *ctx, int row, int col, int len, vui_color_t fg, vui_color_t bg);
void vui_vline(vui_ctx_t *ctx, int row, int col, int len, vui_color_t fg, vui_color_t bg);
void vui_box(vui_ctx_t *ctx, int row, int col, int h, int w, vui_color_t fg, vui_color_t bg);
void vui_fill(vui_ctx_t *ctx, int row, int col, int h, int w, vui_color_t bg);
void vui_set_cell(vui_ctx_t *ctx, int row, int col, uint32_t codepoint,
                   vui_color_t fg, vui_color_t bg, uint8_t attrs);

/* ============================================================
 * Widgets
 * ============================================================ */

/* Label: plain text */
void vui_label(vui_ctx_t *ctx, int row, int col, const char *text,
                vui_color_t fg);

/* Button: returns true if clicked */
bool vui_button(vui_ctx_t *ctx, int row, int col, const char *label,
                 vui_color_t fg, vui_color_t bg);

/* Selectable list item: returns true if clicked */
bool vui_list_item(vui_ctx_t *ctx, int row, int col, int width,
                    const char *text, bool selected, bool highlighted);

/* Text input field: edits buffer in-place, returns true on Enter */
bool vui_input(vui_ctx_t *ctx, int row, int col, int width,
                char *buffer, int buf_size, int *cursor_pos);

/* Scrollbar indicator */
void vui_scrollbar(vui_ctx_t *ctx, int row, int col, int height,
                    int visible, int total, int offset);

/* Progress bar */
void vui_progress(vui_ctx_t *ctx, int row, int col, int width,
                   float value, vui_color_t fg, vui_color_t bg);

/* Section header with line */
void vui_section(vui_ctx_t *ctx, int row, int col, int width,
                  const char *title, vui_color_t fg);

/* Checkbox: renders [x] or [ ] with label, toggles *value on click, returns true if changed */
bool vui_checkbox(vui_ctx_t *ctx, int row, int col, const char *label,
                   bool *value);

/* Dropdown: renders current selection, opens list on click.
 * *selected is the index of the currently selected item.
 * *open tracks whether the dropdown list is visible.
 * Returns true if selection changed. */
bool vui_dropdown(vui_ctx_t *ctx, int row, int col, int width,
                   const char **items, int item_count,
                   int *selected, bool *open);

/* Labeled value: "Label:  value" with hover highlight for the value area.
 * Returns true if clicked (for inline editing). */
bool vui_field_label(vui_ctx_t *ctx, int row, int col, int label_w,
                      const char *label, const char *value, int value_w);

/* Tooltip: shows help text when hovering over the given area */
void vui_tooltip(vui_ctx_t *ctx, int hover_row, int hover_col, int hover_w,
                  const char *text);

/* Slider: horizontal bar from min to max, adjustable via click.
 * Returns true if value changed. */
bool vui_slider(vui_ctx_t *ctx, int row, int col, int width,
                 float *value, float min, float max, const char *fmt);

/* Radio button group: renders ( ) options, returns true if changed */
bool vui_radio(vui_ctx_t *ctx, int row, int col,
                const char **labels, int count, int *selected);

/* Keybind capture field: shows current keybind, captures next keypress on focus */
bool vui_keybind_input(vui_ctx_t *ctx, int row, int col, int width,
                        char *buffer, int buf_size, bool *capturing);

#endif /* VGP_UI_H */