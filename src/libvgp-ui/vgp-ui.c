#include "vgp-ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

/* ============================================================
 * Event handler
 * ============================================================ */

static void on_event(vgp_connection_t *conn, const vgp_event_t *ev, void *data)
{
    vui_ctx_t *ctx = data;
    (void)conn;

    switch (ev->type) {
    case VGP_EVENT_KEY_PRESS:
        ctx->last_keysym = ev->key.keysym;
        ctx->last_mods = ev->key.modifiers;
        memcpy(ctx->last_utf8, ev->key.utf8, sizeof(ctx->last_utf8));
        ctx->key_pressed = true;
        ctx->dirty = true;
        break;
    case VGP_EVENT_MOUSE_BUTTON:
        ctx->mouse_row = (int)(ev->mouse_button.y / 20.0f);
        ctx->mouse_col = (int)(ev->mouse_button.x / 9.0f);
        if (ev->mouse_button.pressed && !ctx->mouse_pressed)
            ctx->mouse_clicked = true;
        ctx->mouse_pressed = ev->mouse_button.pressed;
        ctx->dirty = true;
        break;
    case VGP_EVENT_MOUSE_MOVE:
        ctx->mouse_row = (int)(ev->mouse_move.y / 20.0f);
        ctx->mouse_col = (int)(ev->mouse_move.x / 9.0f);
        ctx->dirty = true;
        break;
    case VGP_EVENT_MOUSE_SCROLL:
        ctx->scroll_offset -= (int)(ev->scroll.dy / 15.0f);
        if (ctx->scroll_offset < 0) ctx->scroll_offset = 0;
        ctx->dirty = true;
        break;
    case VGP_EVENT_CONFIGURE:
        ctx->cols = (int)(ev->configure.width / 9.0f);
        ctx->rows = (int)(ev->configure.height / 20.0f);
        if (ctx->cols > VUI_MAX_COLS) ctx->cols = VUI_MAX_COLS;
        if (ctx->rows > VUI_MAX_ROWS) ctx->rows = VUI_MAX_ROWS;
        ctx->dirty = true;
        break;
    case VGP_EVENT_CLOSE:
        ctx->running = false;
        break;
    default:
        break;
    }
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

int vui_init(vui_ctx_t *ctx, const char *title, int width, int height)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->cols = width / 9;
    ctx->rows = height / 20;
    if (ctx->cols > VUI_MAX_COLS) ctx->cols = VUI_MAX_COLS;
    if (ctx->rows > VUI_MAX_ROWS) ctx->rows = VUI_MAX_ROWS;

    signal(SIGPIPE, SIG_IGN);

    ctx->conn = vgp_connect(NULL);
    if (!ctx->conn) {
        fprintf(stderr, "vgp-ui: cannot connect to VGP server\n");
        return -1;
    }

    vgp_set_event_callback(ctx->conn, on_event, ctx);

    ctx->window_id = vgp_window_create(ctx->conn, -1, -1,
                                         (uint32_t)width, (uint32_t)height,
                                         title, VGP_WINDOW_DECORATED | VGP_WINDOW_RESIZABLE);
    if (!ctx->window_id) {
        fprintf(stderr, "vgp-ui: cannot create window\n");
        vgp_disconnect(ctx->conn);
        return -1;
    }

    ctx->running = true;
    ctx->dirty = true;
    ctx->selected_item = -1;
    return 0;
}

void vui_destroy(vui_ctx_t *ctx)
{
    if (ctx->window_id && ctx->conn)
        vgp_window_destroy(ctx->conn, ctx->window_id);
    if (ctx->conn)
        vgp_disconnect(ctx->conn);
}

void vui_begin_frame(vui_ctx_t *ctx)
{
    memset(ctx->cells, 0, sizeof(ctx->cells));
    ctx->key_pressed = false;
    ctx->mouse_clicked = false;
}

void vui_end_frame(vui_ctx_t *ctx)
{
    vgp_cellgrid_send(ctx->conn, ctx->window_id,
                       (uint16_t)ctx->rows, (uint16_t)ctx->cols,
                       0, 0, 0, 0, ctx->cells);
}

int vui_poll(vui_ctx_t *ctx, int timeout_ms)
{
    struct pollfd pfd = { .fd = vgp_fd(ctx->conn), .events = POLLIN };
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        if (vgp_dispatch(ctx->conn) < 0) {
            ctx->running = false;
            return -1;
        }
    }
    return ret;
}

void vui_run(vui_ctx_t *ctx, void (*render)(vui_ctx_t *ctx))
{
    while (ctx->running) {
        vui_poll(ctx, 16);

        if (ctx->dirty) {
            vui_begin_frame(ctx);
            render(ctx);
            vui_end_frame(ctx);
            ctx->dirty = false;
        }
    }
}

/* ============================================================
 * Drawing primitives
 * ============================================================ */

void vui_set_cell(vui_ctx_t *ctx, int row, int col, uint32_t cp,
                   vui_color_t fg, vui_color_t bg, uint8_t attrs)
{
    if (row < 0 || row >= ctx->rows || col < 0 || col >= ctx->cols) return;
    vgp_cell_t *c = &ctx->cells[row * ctx->cols + col];
    c->codepoint = cp;
    c->fg_r = fg.r; c->fg_g = fg.g; c->fg_b = fg.b;
    c->bg_r = bg.r; c->bg_g = bg.g; c->bg_b = bg.b;
    c->attrs = attrs;
    c->width = 1;
}

void vui_clear(vui_ctx_t *ctx, vui_color_t bg)
{
    for (int r = 0; r < ctx->rows; r++)
        for (int c = 0; c < ctx->cols; c++)
            vui_set_cell(ctx, r, c, ' ', bg, bg, 0);
}

void vui_text(vui_ctx_t *ctx, int row, int col, const char *text,
               vui_color_t fg, vui_color_t bg)
{
    for (int i = 0; text[i] && col + i < ctx->cols; i++)
        vui_set_cell(ctx, row, col + i, (uint32_t)(unsigned char)text[i], fg, bg, 0);
}

void vui_text_bold(vui_ctx_t *ctx, int row, int col, const char *text,
                    vui_color_t fg, vui_color_t bg)
{
    for (int i = 0; text[i] && col + i < ctx->cols; i++)
        vui_set_cell(ctx, row, col + i, (uint32_t)(unsigned char)text[i], fg, bg, VGP_CELL_BOLD);
}

void vui_hline(vui_ctx_t *ctx, int row, int col, int len, vui_color_t fg, vui_color_t bg)
{
    for (int i = 0; i < len && col + i < ctx->cols; i++)
        vui_set_cell(ctx, row, col + i, 0x2500, fg, bg, 0); /* ─ */
}

void vui_vline(vui_ctx_t *ctx, int row, int col, int len, vui_color_t fg, vui_color_t bg)
{
    for (int i = 0; i < len && row + i < ctx->rows; i++)
        vui_set_cell(ctx, row + i, col, 0x2502, fg, bg, 0); /* │ */
}

void vui_box(vui_ctx_t *ctx, int row, int col, int h, int w, vui_color_t fg, vui_color_t bg)
{
    vui_set_cell(ctx, row, col, 0x250C, fg, bg, 0);           /* ┌ */
    vui_set_cell(ctx, row, col + w - 1, 0x2510, fg, bg, 0);   /* ┐ */
    vui_set_cell(ctx, row + h - 1, col, 0x2514, fg, bg, 0);   /* └ */
    vui_set_cell(ctx, row + h - 1, col + w - 1, 0x2518, fg, bg, 0); /* ┘ */
    vui_hline(ctx, row, col + 1, w - 2, fg, bg);
    vui_hline(ctx, row + h - 1, col + 1, w - 2, fg, bg);
    vui_vline(ctx, row + 1, col, h - 2, fg, bg);
    vui_vline(ctx, row + 1, col + w - 1, h - 2, fg, bg);
}

void vui_fill(vui_ctx_t *ctx, int row, int col, int h, int w, vui_color_t bg)
{
    for (int r = row; r < row + h && r < ctx->rows; r++)
        for (int c = col; c < col + w && c < ctx->cols; c++)
            vui_set_cell(ctx, r, c, ' ', bg, bg, 0);
}

/* ============================================================
 * Widgets
 * ============================================================ */

void vui_label(vui_ctx_t *ctx, int row, int col, const char *text, vui_color_t fg)
{
    vui_color_t bg = {ctx->cells[row * ctx->cols + col].bg_r,
                       ctx->cells[row * ctx->cols + col].bg_g,
                       ctx->cells[row * ctx->cols + col].bg_b};
    vui_text(ctx, row, col, text, fg, bg);
}

bool vui_button(vui_ctx_t *ctx, int row, int col, const char *label,
                 vui_color_t fg, vui_color_t bg)
{
    int len = (int)strlen(label);
    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= col && ctx->mouse_col < col + len + 4);
    bool clicked = hover && ctx->mouse_clicked;

    vui_color_t actual_bg = hover ? (vui_color_t){bg.r + 30, bg.g + 30, bg.b + 30} : bg;
    vui_text(ctx, row, col, " [", fg, actual_bg);
    vui_text(ctx, row, col + 2, label, fg, actual_bg);
    vui_text(ctx, row, col + 2 + len, "] ", fg, actual_bg);

    return clicked;
}

bool vui_list_item(vui_ctx_t *ctx, int row, int col, int width,
                    const char *text, bool selected, bool highlighted)
{
    vui_color_t fg = selected ? VUI_WHITE : (highlighted ? VUI_WHITE : VUI_GRAY);
    vui_color_t bg = selected ? VUI_ACCENT : (highlighted ? VUI_SURFACE : VUI_BG);

    vui_fill(ctx, row, col, 1, width, bg);

    /* Selection indicator */
    if (selected)
        vui_set_cell(ctx, row, col, 0x25B6, VUI_ACCENT, bg, 0); /* ▶ */

    /* Truncated text */
    char buf[256];
    int max_chars = width - 3;
    if (max_chars > 0) {
        snprintf(buf, sizeof(buf), " %.*s", max_chars, text);
        vui_text(ctx, row, col + 1, buf, fg, bg);
    }

    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= col && ctx->mouse_col < col + width);
    return hover && ctx->mouse_clicked;
}

bool vui_input(vui_ctx_t *ctx, int row, int col, int width,
                char *buffer, int buf_size, int *cursor_pos)
{
    vui_color_t fg = VUI_WHITE;
    vui_color_t bg = VUI_SURFACE;
    vui_fill(ctx, row, col, 1, width, bg);
    vui_text(ctx, row, col + 1, buffer, fg, bg);

    /* Cursor */
    int cpos = cursor_pos ? *cursor_pos : (int)strlen(buffer);
    if (cpos < width - 1)
        vui_set_cell(ctx, row, col + 1 + cpos, '_', VUI_ACCENT, bg, VGP_CELL_BLINK);

    /* Handle typing */
    if (ctx->key_pressed) {
        int len = (int)strlen(buffer);
        if (ctx->last_keysym == 0xFF08 && len > 0) { /* Backspace */
            buffer[len - 1] = '\0';
            if (cursor_pos && *cursor_pos > 0) (*cursor_pos)--;
            return false;
        }
        if (ctx->last_keysym == 0xFF0D) return true; /* Enter */
        if (ctx->last_utf8[0] >= 0x20 && len < buf_size - 1) {
            buffer[len] = ctx->last_utf8[0];
            buffer[len + 1] = '\0';
            if (cursor_pos) (*cursor_pos)++;
        }
    }
    return false;
}

void vui_scrollbar(vui_ctx_t *ctx, int row, int col, int height,
                    int visible, int total, int offset)
{
    if (total <= visible) return;
    float ratio = (float)visible / (float)total;
    int bar_h = (int)(ratio * (float)height);
    if (bar_h < 1) bar_h = 1;
    int bar_y = (int)(((float)offset / (float)(total - visible)) * (float)(height - bar_h));

    for (int i = 0; i < height; i++) {
        bool in_bar = (i >= bar_y && i < bar_y + bar_h);
        vui_set_cell(ctx, row + i, col, in_bar ? 0x2588 : 0x2591, /* █ or ░ */
                  VUI_GRAY, VUI_BG, 0);
    }
}

void vui_progress(vui_ctx_t *ctx, int row, int col, int width,
                   float value, vui_color_t fg, vui_color_t bg)
{
    int filled = (int)(value * (float)width);
    for (int i = 0; i < width; i++) {
        vui_set_cell(ctx, row, col + i, i < filled ? 0x2588 : 0x2591, /* █ or ░ */
                  i < filled ? fg : bg, VUI_BG, 0);
    }
}

void vui_section(vui_ctx_t *ctx, int row, int col, int width,
                  const char *title, vui_color_t fg)
{
    int len = (int)strlen(title);
    vui_text_bold(ctx, row, col, title, fg, VUI_BG);
    vui_hline(ctx, row, col + len + 1, width - len - 1, VUI_BORDER, VUI_BG);
}
