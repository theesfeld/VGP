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
    case VGP_EVENT_MOUSE_BUTTON: {
        float cw = ctx->cell_px_w > 0 ? ctx->cell_px_w : 9.0f;
        float ch = ctx->cell_px_h > 0 ? ctx->cell_px_h : 20.0f;
        ctx->mouse_row = (int)(ev->mouse_button.y / ch);
        ctx->mouse_col = (int)(ev->mouse_button.x / cw);
        if (ev->mouse_button.pressed && !ctx->mouse_pressed)
            ctx->mouse_clicked = true;
        ctx->mouse_pressed = ev->mouse_button.pressed;
        ctx->dirty = true;
        break;
    }
    case VGP_EVENT_MOUSE_MOVE: {
        float cw = ctx->cell_px_w > 0 ? ctx->cell_px_w : 9.0f;
        float ch = ctx->cell_px_h > 0 ? ctx->cell_px_h : 20.0f;
        ctx->mouse_row = (int)(ev->mouse_move.y / ch);
        ctx->mouse_col = (int)(ev->mouse_move.x / cw);
        ctx->dirty = true;
        break;
    }
    case VGP_EVENT_MOUSE_SCROLL:
        ctx->scroll_offset -= (int)(ev->scroll.dy / 15.0f);
        if (ctx->scroll_offset < 0) ctx->scroll_offset = 0;
        ctx->dirty = true;
        break;
    case VGP_EVENT_CONFIGURE:
        ctx->pixel_w = (float)ev->configure.width;
        ctx->pixel_h = (float)ev->configure.height;
        ctx->cols = (int)(ctx->pixel_w / 9.0f);
        ctx->rows = (int)(ctx->pixel_h / 20.0f);
        if (ctx->cols > VUI_MAX_COLS) ctx->cols = VUI_MAX_COLS;
        if (ctx->rows > VUI_MAX_ROWS) ctx->rows = VUI_MAX_ROWS;
        if (ctx->cols < 10) ctx->cols = 10;
        if (ctx->rows < 5) ctx->rows = 5;
        /* Update pixel-to-cell ratio */
        ctx->cell_px_w = ctx->pixel_w / (float)ctx->cols;
        ctx->cell_px_h = ctx->pixel_h / (float)ctx->rows;
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
    ctx->pixel_w = (float)width;
    ctx->pixel_h = (float)height;
    ctx->cell_px_w = ctx->pixel_w / (float)ctx->cols;
    ctx->cell_px_h = ctx->pixel_h / (float)ctx->rows;
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

bool vui_checkbox(vui_ctx_t *ctx, int row, int col, const char *label,
                   bool *value)
{
    int total_w = 4 + (int)strlen(label);
    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= col && ctx->mouse_col < col + total_w);
    vui_color_t fg = hover ? VUI_WHITE : VUI_GRAY;
    vui_color_t bg = VUI_BG;

    /* Box */
    if (*value) {
        vui_set_cell(ctx, row, col, '[', fg, bg, 0);
        vui_set_cell(ctx, row, col + 1, 0x2713, VUI_GREEN, bg, VGP_CELL_BOLD); /* checkmark */
        vui_set_cell(ctx, row, col + 2, ']', fg, bg, 0);
    } else {
        vui_set_cell(ctx, row, col, '[', fg, bg, 0);
        vui_set_cell(ctx, row, col + 1, ' ', fg, bg, 0);
        vui_set_cell(ctx, row, col + 2, ']', fg, bg, 0);
    }
    /* Label */
    vui_text(ctx, row, col + 4, label, hover ? VUI_WHITE : VUI_GRAY, bg);

    if (hover && ctx->mouse_clicked) {
        *value = !*value;
        return true;
    }
    return false;
}

bool vui_dropdown(vui_ctx_t *ctx, int row, int col, int width,
                   const char **items, int item_count,
                   int *selected, bool *open)
{
    bool changed = false;
    vui_color_t bg = VUI_SURFACE;
    bool hover_main = (ctx->mouse_row == row &&
                        ctx->mouse_col >= col && ctx->mouse_col < col + width);

    /* Render current selection */
    vui_fill(ctx, row, col, 1, width, bg);
    const char *current = (*selected >= 0 && *selected < item_count) ?
                           items[*selected] : "---";
    char display[128];
    int max_text = width - 4;
    snprintf(display, sizeof(display), " %.*s", max_text, current);
    vui_text(ctx, row, col, display, hover_main ? VUI_WHITE : VUI_GRAY, bg);
    /* Down arrow indicator */
    vui_set_cell(ctx, row, col + width - 2, 0x25BC, VUI_ACCENT, bg, 0); /* triangle down */

    /* Toggle open/closed on click */
    if (hover_main && ctx->mouse_clicked) {
        *open = !*open;
    }

    /* Render dropdown list when open */
    if (*open) {
        for (int i = 0; i < item_count && row + 1 + i < ctx->rows - 1; i++) {
            int item_row = row + 1 + i;
            bool item_hover = (ctx->mouse_row == item_row &&
                                ctx->mouse_col >= col && ctx->mouse_col < col + width);
            bool is_selected = (i == *selected);
            vui_color_t ibg = item_hover ? VUI_ACCENT :
                               (is_selected ? VUI_SURFACE : (vui_color_t){0x18, 0x18, 0x28});
            vui_color_t ifg = item_hover ? VUI_WHITE :
                               (is_selected ? VUI_ACCENT : VUI_GRAY);

            vui_fill(ctx, item_row, col, 1, width, ibg);
            char item_text[128];
            snprintf(item_text, sizeof(item_text), " %s%.*s",
                     is_selected ? "> " : "  ", max_text - 2, items[i]);
            vui_text(ctx, item_row, col, item_text, ifg, ibg);

            if (item_hover && ctx->mouse_clicked) {
                *selected = i;
                *open = false;
                changed = true;
            }
        }
        /* Click outside closes */
        if (ctx->mouse_clicked && !hover_main) {
            bool in_list = (ctx->mouse_row > row &&
                             ctx->mouse_row <= row + item_count &&
                             ctx->mouse_col >= col &&
                             ctx->mouse_col < col + width);
            if (!in_list) *open = false;
        }
    }

    return changed;
}

bool vui_field_label(vui_ctx_t *ctx, int row, int col, int label_w,
                      const char *label, const char *value, int value_w)
{
    vui_text(ctx, row, col, label, VUI_GRAY, VUI_BG);
    int val_col = col + label_w;
    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= val_col && ctx->mouse_col < val_col + value_w);
    vui_color_t vbg = hover ? VUI_SURFACE : VUI_BG;
    vui_fill(ctx, row, val_col, 1, value_w, vbg);
    vui_text(ctx, row, val_col, value, hover ? VUI_WHITE : VUI_ACCENT, vbg);
    return hover && ctx->mouse_clicked;
}

void vui_tooltip(vui_ctx_t *ctx, int hover_row, int hover_col, int hover_w,
                  const char *text)
{
    bool hovering = (ctx->mouse_row == hover_row &&
                      ctx->mouse_col >= hover_col &&
                      ctx->mouse_col < hover_col + hover_w);
    if (!hovering) return;

    /* Show tooltip below the hover area */
    int tip_row = hover_row + 1;
    int tip_col = hover_col;
    int tip_len = (int)strlen(text) + 2;
    if (tip_col + tip_len > ctx->cols) tip_col = ctx->cols - tip_len;
    if (tip_col < 0) tip_col = 0;
    if (tip_row >= ctx->rows - 1) tip_row = hover_row - 1;

    vui_color_t tip_bg = (vui_color_t){0x30, 0x30, 0x50};
    vui_fill(ctx, tip_row, tip_col, 1, tip_len, tip_bg);
    char buf[256];
    snprintf(buf, sizeof(buf), " %s ", text);
    vui_text(ctx, tip_row, tip_col, buf, VUI_WHITE, tip_bg);
}

bool vui_slider(vui_ctx_t *ctx, int row, int col, int width,
                 float *value, float min, float max, const char *fmt)
{
    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= col && ctx->mouse_col < col + width);
    float range = max - min;
    if (range <= 0) range = 1.0f;
    float norm = (*value - min) / range;
    if (norm < 0) norm = 0;
    if (norm > 1) norm = 1;

    int bar_w = width - 8; /* leave room for value text */
    int filled = (int)(norm * (float)bar_w);

    /* Track */
    for (int i = 0; i < bar_w; i++) {
        bool in_fill = (i <= filled);
        vui_set_cell(ctx, row, col + i,
                      in_fill ? 0x2588 : 0x2591, /* filled or light shade */
                      in_fill ? VUI_ACCENT : VUI_GRAY, VUI_BG, 0);
    }

    /* Value text */
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), fmt ? fmt : "%.1f", *value);
    vui_text(ctx, row, col + bar_w + 1, vbuf,
              hover ? VUI_WHITE : VUI_GRAY, VUI_BG);

    /* Click to set value */
    if (hover && ctx->mouse_clicked && ctx->mouse_col < col + bar_w) {
        float click_pos = (float)(ctx->mouse_col - col) / (float)bar_w;
        if (click_pos < 0) click_pos = 0;
        if (click_pos > 1) click_pos = 1;
        *value = min + click_pos * range;
        return true;
    }
    return false;
}

bool vui_radio(vui_ctx_t *ctx, int row, int col,
                const char **labels, int count, int *selected)
{
    bool changed = false;
    int x = col;
    for (int i = 0; i < count; i++) {
        int label_len = (int)strlen(labels[i]);
        int item_w = 4 + label_len;
        bool hover = (ctx->mouse_row == row &&
                       ctx->mouse_col >= x && ctx->mouse_col < x + item_w);
        bool is_sel = (i == *selected);

        /* Radio bullet */
        vui_set_cell(ctx, row, x, '(', hover ? VUI_WHITE : VUI_GRAY, VUI_BG, 0);
        vui_set_cell(ctx, row, x + 1, is_sel ? 0x25CF : ' ',  /* filled circle or space */
                      is_sel ? VUI_ACCENT : VUI_GRAY, VUI_BG, 0);
        vui_set_cell(ctx, row, x + 2, ')', hover ? VUI_WHITE : VUI_GRAY, VUI_BG, 0);
        vui_text(ctx, row, x + 4, labels[i],
                  hover ? VUI_WHITE : (is_sel ? VUI_ACCENT : VUI_GRAY), VUI_BG);

        if (hover && ctx->mouse_clicked && !is_sel) {
            *selected = i;
            changed = true;
        }
        x += item_w + 2;
    }
    return changed;
}

bool vui_keybind_input(vui_ctx_t *ctx, int row, int col, int width,
                        char *buffer, int buf_size, bool *capturing)
{
    bool hover = (ctx->mouse_row == row &&
                   ctx->mouse_col >= col && ctx->mouse_col < col + width);
    vui_color_t bg = *capturing ? (vui_color_t){0x40, 0x20, 0x20} : VUI_SURFACE;
    vui_color_t fg = *capturing ? VUI_YELLOW : (hover ? VUI_WHITE : VUI_GRAY);

    vui_fill(ctx, row, col, 1, width, bg);
    if (*capturing) {
        vui_text(ctx, row, col + 1, "Press key combo...", VUI_YELLOW, bg);
    } else {
        vui_text(ctx, row, col + 1, buffer, fg, bg);
    }

    /* Click to start capturing */
    if (hover && ctx->mouse_clicked && !*capturing) {
        *capturing = true;
        return false;
    }

    /* Capture next keypress */
    if (*capturing && ctx->key_pressed) {
        /* Build modifier string */
        char combo[64] = "";
        if (ctx->last_mods & 0x40) strcat(combo, "Super+");  /* Mod4 */
        if (ctx->last_mods & 0x04) strcat(combo, "Ctrl+");
        if (ctx->last_mods & 0x08) strcat(combo, "Alt+");
        if (ctx->last_mods & 0x01) strcat(combo, "Shift+");

        /* Escape cancels */
        if (ctx->last_keysym == 0xFF1B) {
            *capturing = false;
            return false;
        }

        /* Skip lone modifier presses */
        if (ctx->last_keysym >= 0xFFE1 && ctx->last_keysym <= 0xFFEE)
            return false;

        /* Append key name (simplified) */
        char key_name[32] = "?";
        if (ctx->last_keysym == 0xFF0D) snprintf(key_name, sizeof(key_name), "Return");
        else if (ctx->last_keysym == 0xFF09) snprintf(key_name, sizeof(key_name), "Tab");
        else if (ctx->last_keysym == 0xFF08) snprintf(key_name, sizeof(key_name), "BackSpace");
        else if (ctx->last_keysym == 0xFFFF) snprintf(key_name, sizeof(key_name), "Delete");
        else if (ctx->last_keysym == 0xFF50) snprintf(key_name, sizeof(key_name), "Home");
        else if (ctx->last_keysym == 0xFF57) snprintf(key_name, sizeof(key_name), "End");
        else if (ctx->last_keysym == 0xFF61) snprintf(key_name, sizeof(key_name), "Print");
        else if (ctx->last_keysym >= 0xFF51 && ctx->last_keysym <= 0xFF54) {
            const char *arrows[] = {"Left", "Up", "Right", "Down"};
            snprintf(key_name, sizeof(key_name), "%s", arrows[ctx->last_keysym - 0xFF51]);
        } else if (ctx->last_keysym >= 0x20 && ctx->last_keysym < 0x7F) {
            key_name[0] = (char)ctx->last_keysym;
            if (key_name[0] >= 'a' && key_name[0] <= 'z')
                key_name[0] -= 32; /* uppercase */
            key_name[1] = '\0';
        } else if (ctx->last_keysym >= 0xFFBE && ctx->last_keysym <= 0xFFC9) {
            snprintf(key_name, sizeof(key_name), "F%d", ctx->last_keysym - 0xFFBE + 1);
        }

        strncat(combo, key_name, sizeof(combo) - strlen(combo) - 1);
        snprintf(buffer, (size_t)buf_size, "%s", combo);
        *capturing = false;
        return true;
    }
    return false;
}
