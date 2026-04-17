/* SPDX-License-Identifier: MIT */
#include "term.h"
#include "vgp/xdg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>

#define PTY_READ_CHUNK 65536

/* ============================================================
 * Terminal config loader
 * ============================================================ */

static void trim_str(char *s)
{
    while (isspace(*s)) memmove(s, s + 1, strlen(s));
    size_t len = strlen(s);
    while (len > 0 && isspace(s[len - 1])) s[--len] = '\0';
}

static void config_load_defaults(vgp_term_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->scrollback_lines = 10000;
    cfg->font_size = 14.0f;
    cfg->cursor_style = 1;
    cfg->cursor_blink = true;
    cfg->cursor_blink_ms = 500;
    cfg->padding = 4;
    cfg->selection_bg = 0x5294E2;
    cfg->selection_fg = 0xFFFFFF;
}

static void config_load(vgp_term_config_t *cfg, const char *path)
{
    config_load_defaults(cfg);
    char buf[512];
    if (!path) {
        if (!vgp_xdg_find_config("vgp/terminal.toml", buf, sizeof(buf)))
            return;
        path = buf;
    }

    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[512], section[64] = "";
    while (fgets(line, sizeof(line), f)) {
        trim_str(line);
        if (line[0] == '#' || line[0] == '\0') continue;
        if (line[0] == '[') {
            char *end = strchr(line, ']');
            if (end) { *end = '\0'; snprintf(section, sizeof(section), "%s", line + 1); }
            continue;
        }
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line, *val = eq + 1;
        trim_str(key); trim_str(val);
        /* Strip quotes */
        size_t vlen = strlen(val);
        if (vlen >= 2 && val[0] == '"' && val[vlen-1] == '"') { val[vlen-1] = '\0'; val++; }

        if (strcmp(section, "general") == 0) {
            if (strcmp(key, "shell") == 0 && val[0])
                snprintf(cfg->shell, sizeof(cfg->shell), "%s", val);
            else if (strcmp(key, "scrollback") == 0)
                cfg->scrollback_lines = atoi(val);
            else if (strcmp(key, "font_size") == 0)
                cfg->font_size = strtof(val, NULL);
            else if (strcmp(key, "cursor_style") == 0) {
                if (strcmp(val, "block") == 0) cfg->cursor_style = 1;
                else if (strcmp(val, "underline") == 0) cfg->cursor_style = 2;
                else if (strcmp(val, "bar") == 0) cfg->cursor_style = 3;
            }
            else if (strcmp(key, "cursor_blink") == 0)
                cfg->cursor_blink = strcmp(val, "true") == 0;
            else if (strcmp(key, "padding") == 0)
                cfg->padding = atoi(val);
        }
    }
    fclose(f);
    fprintf(stderr, "  config: font=%.1f scrollback=%d cursor=%d\n",
            cfg->font_size, cfg->scrollback_lines, cfg->cursor_style);
}

/* ============================================================
 * Scrollback buffer
 * ============================================================ */

static void scrollback_init(vgp_term_t *term, int capacity)
{
    term->scrollback.capacity = capacity;
    term->scrollback.lines = calloc((size_t)capacity, sizeof(VTermScreenCell *));
    term->scrollback.line_cols = calloc((size_t)capacity, sizeof(int));
    term->scrollback.count = 0;
    term->scrollback.head = 0;
    term->scrollback.scroll_offset = 0;
}

static void scrollback_destroy(vgp_term_t *term)
{
    if (term->scrollback.lines) {
        for (int i = 0; i < term->scrollback.capacity; i++)
            free(term->scrollback.lines[i]);
        free(term->scrollback.lines);
        free(term->scrollback.line_cols);
    }
}

static void scrollback_push(vgp_term_t *term, int cols, const VTermScreenCell *cells)
{
    int idx = term->scrollback.head;
    free(term->scrollback.lines[idx]);
    term->scrollback.lines[idx] = malloc((size_t)cols * sizeof(VTermScreenCell));
    if (term->scrollback.lines[idx])
        memcpy(term->scrollback.lines[idx], cells, (size_t)cols * sizeof(VTermScreenCell));
    term->scrollback.line_cols[idx] = cols;
    term->scrollback.head = (idx + 1) % term->scrollback.capacity;
    if (term->scrollback.count < term->scrollback.capacity)
        term->scrollback.count++;
}

/* ============================================================
 * Selection
 * ============================================================ */

static void selection_clear(vgp_term_t *term)
{
    free(term->selection.text);
    term->selection.text = NULL;
    term->selection.text_len = 0;
    term->selection.active = false;
    term->selection.valid = false;
}

static void selection_start(vgp_term_t *term, int row, int col)
{
    selection_clear(term);
    term->selection.start_row = row;
    term->selection.start_col = col;
    term->selection.end_row = row;
    term->selection.end_col = col;
    term->selection.active = true;
}

static void selection_update(vgp_term_t *term, int row, int col)
{
    term->selection.end_row = row;
    term->selection.end_col = col;
    term->render_pending = true;
}

static void selection_finish(vgp_term_t *term)
{
    term->selection.active = false;
    if (term->selection.start_row == term->selection.end_row &&
        term->selection.start_col == term->selection.end_col) {
        term->selection.valid = false;
        return;
    }
    term->selection.valid = true;

    /* Extract selected text */
    int sr = term->selection.start_row, sc = term->selection.start_col;
    int er = term->selection.end_row, ec = term->selection.end_col;

    /* Normalize: start before end */
    if (sr > er || (sr == er && sc > ec)) {
        int tr = sr, tc = sc;
        sr = er; sc = ec;
        er = tr; ec = tc;
    }

    /* Build text from cells */
    size_t cap = 4096;
    char *buf = malloc(cap);
    size_t len = 0;
    if (!buf) return;

    for (int row = sr; row <= er; row++) {
        int col_start = (row == sr) ? sc : 0;
        int col_end = (row == er) ? ec : term->cols - 1;

        for (int col = col_start; col <= col_end && col < term->cols; col++) {
            VTermPos pos = { .row = row, .col = col };
            VTermScreenCell cell;
            vterm_screen_get_cell(term->vt_screen, pos, &cell);

            if (cell.chars[0] == 0) {
                if (len + 1 < cap) buf[len++] = ' ';
            } else {
                uint32_t cp = cell.chars[0];
                char utf8[5] = {0};
                int ulen = 0;
                if (cp < 0x80) { utf8[0] = (char)cp; ulen = 1; }
                else if (cp < 0x800) {
                    utf8[0] = (char)(0xC0 | (cp >> 6));
                    utf8[1] = (char)(0x80 | (cp & 0x3F));
                    ulen = 2;
                } else if (cp < 0x10000) {
                    utf8[0] = (char)(0xE0 | (cp >> 12));
                    utf8[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    utf8[2] = (char)(0x80 | (cp & 0x3F));
                    ulen = 3;
                } else {
                    utf8[0] = (char)(0xF0 | (cp >> 18));
                    utf8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
                    utf8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    utf8[3] = (char)(0x80 | (cp & 0x3F));
                    ulen = 4;
                }
                if (len + (size_t)ulen < cap)
                    memcpy(buf + len, utf8, (size_t)ulen);
                len += (size_t)ulen;
            }
        }
        if (row < er && len + 1 < cap)
            buf[len++] = '\n';
    }
    buf[len] = '\0';

    free(term->selection.text);
    term->selection.text = buf;
    term->selection.text_len = len;
}

/* ============================================================
 * libvterm callbacks
 * ============================================================ */

static int on_damage(VTermRect rect, void *user)
{
    vgp_term_t *term = user;
    if (!term->damage.dirty_rows) {
        term->damage.full_redraw = true;
        return 0;
    }
    for (int row = rect.start_row; row < rect.end_row && row < term->rows; row++) {
        term->damage.dirty_rows[row] = true;
        term->damage.dirty_count++;
    }
    term->render_pending = true;
    return 0;
}

static int on_moverect(VTermRect dest, VTermRect src, void *user)
{
    vgp_term_t *term = user;
    (void)dest; (void)src;
    /* For simplicity, mark full redraw on scroll.
     * Optimization: memmove pixel data and only redraw new rows. */
    term->damage.full_redraw = true;
    term->render_pending = true;
    return 0;
}

static int on_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
    vgp_term_t *term = user;
    (void)oldpos;
    term->cursor.pos = pos;
    term->cursor.visible = visible != 0;
    term->cursor.blink_state = true;

    /* Mark old and new cursor rows as dirty */
    if (oldpos.row >= 0 && oldpos.row < term->rows)
        term->damage.dirty_rows[oldpos.row] = true;
    if (pos.row >= 0 && pos.row < term->rows)
        term->damage.dirty_rows[pos.row] = true;
    term->render_pending = true;
    return 0;
}

static int on_settermprop(VTermProp prop, VTermValue *val, void *user)
{
    vgp_term_t *term = user;

    switch (prop) {
    case VTERM_PROP_TITLE:
        if (val->string.len > 0 && term->conn && term->window_id) {
            /* Copy title with null-termination */
            char title[256];
            size_t len = val->string.len;
            if (len >= sizeof(title)) len = sizeof(title) - 1;
            memcpy(title, val->string.str, len);
            title[len] = '\0';
            vgp_window_set_title(term->conn, term->window_id, title);
        }
        break;
    case VTERM_PROP_CURSORVISIBLE:
        term->cursor.visible = val->boolean;
        break;
    case VTERM_PROP_CURSORSHAPE:
        term->cursor.shape = val->number;
        break;
    default:
        break;
    }
    return 1;
}

static int on_bell(void *user)
{
    (void)user;
    return 0;
}

static void vterm_output_cb(const char *s, size_t len, void *user)
{
    vgp_term_t *term = user;
    vgp_term_pty_write(term, s, len);
}

static int on_sb_pushline(int cols, const VTermScreenCell *cells, void *user)
{
    vgp_term_t *term = user;
    scrollback_push(term, cols, cells);
    return 1;
}

static int on_sb_popline(int cols, VTermScreenCell *cells, void *user)
{
    vgp_term_t *term = user;
    if (term->scrollback.count == 0) return 0;

    /* Pop the most recent line */
    int idx = (term->scrollback.head - 1 + term->scrollback.capacity)
              % term->scrollback.capacity;
    if (term->scrollback.lines[idx]) {
        int copy_cols = term->scrollback.line_cols[idx];
        if (copy_cols > cols) copy_cols = cols;
        memcpy(cells, term->scrollback.lines[idx], (size_t)copy_cols * sizeof(VTermScreenCell));
        /* Clear remaining cells */
        for (int c = copy_cols; c < cols; c++)
            memset(&cells[c], 0, sizeof(VTermScreenCell));
        free(term->scrollback.lines[idx]);
        term->scrollback.lines[idx] = NULL;
    }
    term->scrollback.head = idx;
    term->scrollback.count--;
    return 1;
}

static VTermScreenCallbacks screen_callbacks = {
    .damage      = on_damage,
    .moverect    = on_moverect,
    .movecursor  = on_movecursor,
    .settermprop = on_settermprop,
    .bell        = on_bell,
    .sb_pushline = on_sb_pushline,
    .sb_popline  = on_sb_popline,
};

/* ============================================================
 * VGP event handler
 * ============================================================ */

static void on_vgp_event(vgp_connection_t *conn, const vgp_event_t *ev, void *data)
{
    vgp_term_t *term = data;
    (void)conn;

    switch (ev->type) {
    case VGP_EVENT_KEY_PRESS: {
        uint32_t mods = ev->key.modifiers;
        uint32_t sym = ev->key.keysym;
        bool shift = (mods & 0x0001) != 0;
        bool ctrl = (mods & 0x0002) != 0;

        /* === Terminal-specific keybinds (consume, don't pass to shell) === */

        /* Ctrl+Shift+C = copy selection to clipboard */
        if (ctrl && shift && (sym == 0x0063 || sym == 0x0043)) { /* 'c' or 'C' */
            if (term->selection.valid && term->selection.text) {
                vgp_clipboard_set(term->conn, term->selection.text,
                                   term->selection.text_len);
                fprintf(stderr, "  copied %zu bytes to clipboard\n",
                        term->selection.text_len);
            }
            break;
        }

        /* Ctrl+Shift+V = paste from clipboard */
        if (ctrl && shift && (sym == 0x0076 || sym == 0x0056)) { /* 'v' or 'V' */
            size_t len;
            char *clip = vgp_clipboard_get(term->conn, &len);
            if (clip) {
                /* Bracketed paste mode */
                vterm_keyboard_start_paste(term->vt);
                vgp_term_pty_write(term, clip, len);
                vterm_keyboard_end_paste(term->vt);
                free(clip);
                fprintf(stderr, "  pasted %zu bytes from clipboard\n", len);
            }
            break;
        }

        /* Ctrl+Shift+F = toggle search */
        if (ctrl && shift && (sym == 0x0066 || sym == 0x0046)) { /* f/F */
            term->search.active = !term->search.active;
            if (term->search.active) {
                term->search.query_len = 0;
                term->search.query[0] = '\0';
                term->search.match_row = -1;
            }
            term->render_pending = true;
            break;
        }

        /* If search is active, capture input for search query */
        if (term->search.active) {
            if (sym == 0xFF1B) { /* Escape: cancel search */
                term->search.active = false;
                term->render_pending = true;
                break;
            }
            if (sym == 0xFF08 && term->search.query_len > 0) { /* Backspace */
                term->search.query[--term->search.query_len] = '\0';
                term->render_pending = true;
                break;
            }
            if (sym == 0xFF0D) { /* Enter: find next */
                /* Search through screen for query */
                if (term->search.query_len > 0) {
                    for (int r = 0; r < term->rows; r++) {
                        for (int c = 0; c < term->cols - term->search.query_len; c++) {
                            bool match = true;
                            for (int q = 0; q < term->search.query_len && match; q++) {
                                VTermPos pos = { .row = r, .col = c + q };
                                VTermScreenCell cell;
                                vterm_screen_get_cell(term->vt_screen, pos, &cell);
                                if ((char)cell.chars[0] != term->search.query[q])
                                    match = false;
                            }
                            if (match) {
                                term->search.match_row = r;
                                term->search.match_col = c;
                                break;
                            }
                        }
                        if (term->search.match_row >= 0) break;
                    }
                }
                term->render_pending = true;
                break;
            }
            if (ev->key.utf8_len > 0 && (unsigned char)ev->key.utf8[0] >= 0x20 &&
                term->search.query_len < 255) {
                term->search.query[term->search.query_len++] = ev->key.utf8[0];
                term->search.query[term->search.query_len] = '\0';
                term->render_pending = true;
                break;
            }
            break; /* consume all input while searching */
        }

        /* Ctrl+Plus/Minus/0 = font size */
        if (ctrl && (sym == 0x002B || sym == 0xFFAB)) { /* + or KP_Add */
            term->config.font_size += 1.0f;
            if (term->config.font_size > 36.0f) term->config.font_size = 36.0f;
            /* Tell server about new font size */
            vgp_msg_set_font_size_t msg = {
                .header = { .magic = 0x56475000, .type = 0x0091,
                            .length = sizeof(msg), .window_id = term->window_id },
                .font_size = term->config.font_size,
            };
            send(vgp_fd(term->conn), &msg, sizeof(msg), MSG_NOSIGNAL);
            fprintf(stderr, "  font size: %.0f\n", term->config.font_size);
            break;
        }
        if (ctrl && (sym == 0x002D || sym == 0xFFAD)) { /* - or KP_Subtract */
            term->config.font_size -= 1.0f;
            if (term->config.font_size < 8.0f) term->config.font_size = 8.0f;
            vgp_msg_set_font_size_t msg = {
                .header = { .magic = 0x56475000, .type = 0x0091,
                            .length = sizeof(msg), .window_id = term->window_id },
                .font_size = term->config.font_size,
            };
            send(vgp_fd(term->conn), &msg, sizeof(msg), MSG_NOSIGNAL);
            fprintf(stderr, "  font size: %.0f\n", term->config.font_size);
            break;
        }
        if (ctrl && sym == 0x0030) { /* 0 = reset */
            term->config.font_size = 14.0f;
            vgp_msg_set_font_size_t msg = {
                .header = { .magic = 0x56475000, .type = 0x0091,
                            .length = sizeof(msg), .window_id = term->window_id },
                .font_size = term->config.font_size,
            };
            send(vgp_fd(term->conn), &msg, sizeof(msg), MSG_NOSIGNAL);
            fprintf(stderr, "  font size: reset to %.0f\n", term->config.font_size);
            break;
        }

        /* Shift+PageUp/Down = scroll */
        if (shift && sym == 0xFF55) { /* PageUp */
            int page = term->rows / 2;
            term->scrollback.scroll_offset += page;
            if (term->scrollback.scroll_offset > term->scrollback.count)
                term->scrollback.scroll_offset = term->scrollback.count;
            term->render_pending = true;
            break;
        }
        if (shift && sym == 0xFF56) { /* PageDown */
            int page = term->rows / 2;
            term->scrollback.scroll_offset -= page;
            if (term->scrollback.scroll_offset < 0)
                term->scrollback.scroll_offset = 0;
            term->render_pending = true;
            break;
        }

        /* === Pass through to shell === */
        VTermModifier mod = VTERM_MOD_NONE;
        if (shift) mod |= VTERM_MOD_SHIFT;
        if (ctrl)  mod |= VTERM_MOD_CTRL;
        if (mods & 0x0004) mod |= VTERM_MOD_ALT;

        /* Any keyboard input snaps scroll to bottom */
        if (term->scrollback.scroll_offset > 0) {
            term->scrollback.scroll_offset = 0;
            term->render_pending = true;
        }

        VTermKey vk = VTERM_KEY_NONE;
        switch (ev->key.keysym) {
        case 0xFF08: vk = VTERM_KEY_BACKSPACE; break;
        case 0xFF09: vk = VTERM_KEY_TAB; break;
        case 0xFF0D: vk = VTERM_KEY_ENTER; break;
        case 0xFF1B: vk = VTERM_KEY_ESCAPE; break;
        case 0xFF51: vk = VTERM_KEY_LEFT; break;
        case 0xFF52: vk = VTERM_KEY_UP; break;
        case 0xFF53: vk = VTERM_KEY_RIGHT; break;
        case 0xFF54: vk = VTERM_KEY_DOWN; break;
        case 0xFF55: vk = VTERM_KEY_PAGEUP; break;
        case 0xFF56: vk = VTERM_KEY_PAGEDOWN; break;
        case 0xFF50: vk = VTERM_KEY_HOME; break;
        case 0xFF57: vk = VTERM_KEY_END; break;
        case 0xFF63: vk = VTERM_KEY_INS; break;
        case 0xFFFF: vk = VTERM_KEY_DEL; break;
        case 0xFFBE: vk = VTERM_KEY_FUNCTION(1); break;
        case 0xFFBF: vk = VTERM_KEY_FUNCTION(2); break;
        case 0xFFC0: vk = VTERM_KEY_FUNCTION(3); break;
        case 0xFFC1: vk = VTERM_KEY_FUNCTION(4); break;
        case 0xFFC2: vk = VTERM_KEY_FUNCTION(5); break;
        case 0xFFC3: vk = VTERM_KEY_FUNCTION(6); break;
        case 0xFFC4: vk = VTERM_KEY_FUNCTION(7); break;
        case 0xFFC5: vk = VTERM_KEY_FUNCTION(8); break;
        case 0xFFC6: vk = VTERM_KEY_FUNCTION(9); break;
        case 0xFFC7: vk = VTERM_KEY_FUNCTION(10); break;
        case 0xFFC8: vk = VTERM_KEY_FUNCTION(11); break;
        case 0xFFC9: vk = VTERM_KEY_FUNCTION(12); break;
        default: break;
        }

        if (vk != VTERM_KEY_NONE) {
            vterm_keyboard_key(term->vt, vk, mod);
        } else if (ev->key.utf8_len > 0) {
            /* Send UTF-8 characters */
            for (uint32_t i = 0; i < ev->key.utf8_len; i++)
                vterm_keyboard_unichar(term->vt, (uint32_t)(unsigned char)ev->key.utf8[i], mod);
        }
        break;
    }
    case VGP_EVENT_CONFIGURE: {
        /* Window was resized -- recalculate grid and recreate surface */
        int new_w = (int)ev->configure.width;
        int new_h = (int)ev->configure.height;
        if (new_w < 80) new_w = 80;
        if (new_h < 40) new_h = 40;

        int new_cols = (int)((float)new_w / term->fonts.cell_width);
        int new_rows = (int)((float)new_h / term->fonts.cell_height);
        if (new_cols < 80) new_cols = 80;
        if (new_rows < 24) new_rows = 24;

        if (new_cols != term->cols || new_rows != term->rows) {
            term->cols = new_cols;
            term->rows = new_rows;

            /* Resize damage tracking */
            free(term->damage.dirty_rows);
            term->damage.dirty_rows = calloc((size_t)term->rows, sizeof(bool));
            term->damage.full_redraw = true;

            /* Resize vterm + PTY */
            vterm_set_size(term->vt, term->rows, term->cols);
            vgp_term_pty_resize(term, term->rows, term->cols);

            fprintf(stderr, "  resize: %dx%d\n", term->cols, term->rows);
            term->render_pending = true;
        }
        break;
    }
    case VGP_EVENT_MOUSE_BUTTON: {
        /* Convert pixel position to cell coordinates */
        float cw = (float)1; /* will be computed from content area */
        float ch = (float)1;
        /* We don't know exact pixel-to-cell mapping since server renders.
         * Use approximate: content_w / cols, content_h / rows.
         * The mouse coords from the server are window-relative. */
        if (term->cols > 0) cw = ev->mouse_button.x / (float)term->cols;
        if (term->rows > 0) ch = ev->mouse_button.y / (float)term->rows;
        int col = (int)(ev->mouse_button.x / 9.0f); /* approx cell width */
        int row = (int)(ev->mouse_button.y / 20.0f); /* approx cell height */
        if (col < 0) col = 0;
        if (col >= term->cols) col = term->cols - 1;
        if (row < 0) row = 0;
        if (row >= term->rows) row = term->rows - 1;
        (void)cw; (void)ch;

        if (ev->mouse_button.pressed && ev->mouse_button.button == 0x110) {
            /* Ctrl+click on URL: open it */
            if (ev->mouse_button.modifiers & 0x04) { /* Ctrl */
                /* Scan backward from click position to find URL start */
                VTermScreen *screen = vterm_obtain_screen(term->vt);
                int url_start = col, url_end = col;
                /* Find start of URL (look for http) */
                for (int c = col; c >= 0; c--) {
                    VTermPos pos = {.row = row, .col = c};
                    VTermScreenCell cell;
                    vterm_screen_get_cell(screen, pos, &cell);
                    if (cell.chars[0] <= 32) { url_start = c + 1; break; }
                    if (c == 0) url_start = 0;
                }
                /* Find end of URL */
                for (int c = col; c < term->cols; c++) {
                    VTermPos pos = {.row = row, .col = c};
                    VTermScreenCell cell;
                    vterm_screen_get_cell(screen, pos, &cell);
                    if (cell.chars[0] <= 32 || cell.chars[0] == '"' ||
                        cell.chars[0] == '\'' || cell.chars[0] == '>' ||
                        cell.chars[0] == ')') { url_end = c; break; }
                    url_end = c + 1;
                }
                /* Extract URL text */
                char url[2048] = {0};
                int ui = 0;
                for (int c = url_start; c < url_end && ui < (int)sizeof(url) - 1; c++) {
                    VTermPos pos = {.row = row, .col = c};
                    VTermScreenCell cell;
                    vterm_screen_get_cell(screen, pos, &cell);
                    if (cell.chars[0] > 0 && cell.chars[0] < 128)
                        url[ui++] = (char)cell.chars[0];
                }
                url[ui] = '\0';
                if (strncmp(url, "http", 4) == 0) {
                    vgp_open_url(term->conn, url);
                }
            } else {
                /* Left button press -- start selection */
                selection_start(term, row, col);
            }
        } else if (!ev->mouse_button.pressed && ev->mouse_button.button == 0x110) {
            /* Left button release -- finish selection */
            selection_update(term, row, col);
            selection_finish(term);
            if (term->selection.valid) {
                /* Auto-copy to clipboard on selection */
                vgp_clipboard_set(term->conn, term->selection.text,
                                   term->selection.text_len);
            }
        }
        term->render_pending = true;
        break;
    }
    case VGP_EVENT_MOUSE_MOVE: {
        if (term->selection.active) {
            int col = (int)(ev->mouse_move.x / 9.0f);
            int row = (int)(ev->mouse_move.y / 20.0f);
            if (col < 0) col = 0;
            if (col >= term->cols) col = term->cols - 1;
            if (row < 0) row = 0;
            if (row >= term->rows) row = term->rows - 1;
            selection_update(term, row, col);
        }
        break;
    }
    case VGP_EVENT_MOUSE_SCROLL: {
        /* Scroll terminal with mouse wheel */
        int delta = (int)(ev->scroll.dy / 15.0f);
        if (delta == 0) delta = ev->scroll.dy > 0 ? 1 : -1;
        term->scrollback.scroll_offset += delta;
        if (term->scrollback.scroll_offset > term->scrollback.count)
            term->scrollback.scroll_offset = term->scrollback.count;
        if (term->scrollback.scroll_offset < 0)
            term->scrollback.scroll_offset = 0;
        term->render_pending = true;
        break;
    }
    case VGP_EVENT_CLOSE:
        term->running = false;
        break;
    default:
        break;
    }
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

int vgp_term_init(vgp_term_t *term, int width, int height)
{
    memset(term, 0, sizeof(*term));

    /* Load terminal config */
    config_load(&term->config, NULL);

    fprintf(stderr, "  term_init: loading fonts\n");
    if (vgp_term_fonts_init(&term->fonts, term->config.font_size) < 0) {
        fprintf(stderr, "  term_init: font loading FAILED\n");
        return -1;
    }
    fprintf(stderr, "  term_init: font ok (cell=%.1fx%.1f)\n",
            term->fonts.cell_width, term->fonts.cell_height);

    term->cols = (int)((float)width / term->fonts.cell_width);
    term->rows = (int)((float)height / term->fonts.cell_height);
    if (term->cols < 80) term->cols = 80;
    if (term->rows < 24) term->rows = 24;
    fprintf(stderr, "  term_init: grid=%dx%d\n", term->cols, term->rows);

    vgp_term_palette_init(term);

    /* Allocate damage tracking BEFORE vterm init, because
     * vterm_screen_reset triggers the damage callback immediately */
    term->damage.dirty_rows = calloc((size_t)term->rows, sizeof(bool));
    if (!term->damage.dirty_rows) goto err_font;
    term->damage.full_redraw = true;

    fprintf(stderr, "  term_init: creating vterm %dx%d\n", term->rows, term->cols);
    term->vt = vterm_new(term->rows, term->cols);
    if (!term->vt) { fprintf(stderr, "  term_init: vterm_new FAILED\n"); goto err_damage; }

    vterm_set_utf8(term->vt, 1);
    vterm_output_set_callback(term->vt, vterm_output_cb, term);
    term->vt_screen = vterm_obtain_screen(term->vt);
    vterm_screen_set_callbacks(term->vt_screen, &screen_callbacks, term);
    vterm_screen_set_damage_merge(term->vt_screen, VTERM_DAMAGE_ROW);
    vterm_screen_reset(term->vt_screen, 1);
    term->cursor.visible = true;
    term->cursor.blink_state = true;
    term->cursor.shape = term->config.cursor_style;

    /* Initialize scrollback buffer */
    scrollback_init(term, term->config.scrollback_lines);

    fprintf(stderr, "  term_init: connecting to VGP server\n");
    term->conn = vgp_connect(NULL);
    if (!term->conn) { fprintf(stderr, "  term_init: vgp_connect FAILED\n"); goto err_damage; }
    fprintf(stderr, "  term_init: connected to VGP server\n");

    vgp_set_event_callback(term->conn, on_vgp_event, term);

    /* Request window sized for our grid. The server will use its own
     * font metrics to compute actual pixel dimensions. We send cols/rows
     * encoded as width/height -- the server knows this is a terminal. */
    uint32_t req_w = (uint32_t)(term->cols * 9);  /* approx 9px per col */
    uint32_t req_h = (uint32_t)(term->rows * 20); /* approx 20px per row */
    fprintf(stderr, "  term_init: creating window %ux%u (%dx%d grid)\n",
            req_w, req_h, term->cols, term->rows);
    term->window_id = vgp_window_create(term->conn, -1, -1,
                                          req_w, req_h,
                                          "vgp-term",
                                          VGP_WINDOW_DECORATED | VGP_WINDOW_RESIZABLE);
    if (term->window_id == 0) { fprintf(stderr, "  term_init: window create FAILED\n"); goto err_conn; }
    fprintf(stderr, "  term_init: window created id=%u\n", term->window_id);

    fprintf(stderr, "  term_init: spawning shell\n");
    if (vgp_term_pty_spawn(term) < 0) { fprintf(stderr, "  term_init: pty spawn FAILED\n"); goto err_window; }
    fprintf(stderr, "  term_init: shell spawned pid=%d\n", term->child_pid);

    term->running = true;
    return 0;

err_window:
    vgp_window_destroy(term->conn, term->window_id);
err_conn:
    vgp_disconnect(term->conn);
    term->conn = NULL;
err_damage:
    free(term->damage.dirty_rows);
err_font:
    vgp_term_fonts_destroy(&term->fonts);
    return -1;
}

void vgp_term_destroy(vgp_term_t *term)
{
    if (term->child_pid > 0) {
        kill(term->child_pid, SIGTERM);
        waitpid(term->child_pid, NULL, 0);
    }
    if (term->pty_fd >= 0)
        close(term->pty_fd);
    if (term->window_id && term->conn)
        vgp_window_destroy(term->conn, term->window_id);
    if (term->conn)
        vgp_disconnect(term->conn);
    if (term->vt)
        vterm_free(term->vt);
    scrollback_destroy(term);
    selection_clear(term);
    free(term->damage.dirty_rows);
    vgp_term_fonts_destroy(&term->fonts);
}

void vgp_term_run(vgp_term_t *term)
{
    char pty_buf[PTY_READ_CHUNK];

    fprintf(stderr, "  run: initial render + commit\n");
    vgp_term_render_dirty(term);
    vgp_term_commit_surface(term);
    fprintf(stderr, "  run: entering poll loop (pty_fd=%d, server_fd=%d)\n",
            term->pty_fd, vgp_fd(term->conn));

    int frame_count = 0;
    while (term->running) {
        struct pollfd fds[2] = {
            { .fd = term->pty_fd, .events = POLLIN },
            { .fd = vgp_fd(term->conn), .events = POLLIN },
        };

        int ret = poll(fds, 2, 16);
        if (ret < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "  run: poll error: %s\n", strerror(errno));
            break;
        }

        /* Read PTY output */
        if (fds[0].revents & POLLIN) {
            ssize_t n;
            while ((n = read(term->pty_fd, pty_buf, sizeof(pty_buf))) > 0) {
                vterm_input_write(term->vt, pty_buf, (size_t)n);
            }
            /* n == 0 means EOF (shell exited). n == -1 with EAGAIN is normal. */
            if (n == 0) {
                fprintf(stderr, "  run: PTY EOF (shell exited)\n");
                term->running = false;
                break;
            }
        }

        /* Only treat POLLHUP as exit if POLLIN is NOT set.
         * On Linux, POLLHUP + POLLIN can arrive together when data
         * is available right before the shell exits. Process data first. */
        if ((fds[0].revents & POLLHUP) && !(fds[0].revents & POLLIN)) {
            fprintf(stderr, "  run: PTY POLLHUP (shell gone)\n");
            term->running = false;
            break;
        }

        if (fds[0].revents & POLLERR) {
            fprintf(stderr, "  run: PTY POLLERR\n");
            term->running = false;
            break;
        }

        /* Process VGP server events (keyboard, mouse, etc.) */
        if (fds[1].revents & POLLIN) {
            if (vgp_dispatch(term->conn) < 0) {
                fprintf(stderr, "  run: server disconnected\n");
                term->running = false;
                break;
            }
        }

        if (fds[1].revents & (POLLHUP | POLLERR)) {
            fprintf(stderr, "  run: server connection lost\n");
            term->running = false;
            break;
        }

        /* Flush libvterm damage and render */
        vterm_screen_flush_damage(term->vt_screen);

        if (term->render_pending || term->damage.full_redraw) {
            vgp_term_render_dirty(term);
            vgp_term_commit_surface(term);
            term->render_pending = false;
            frame_count++;
            if (frame_count <= 5)
                fprintf(stderr, "  run: frame %d committed\n", frame_count);
        }
    }
    fprintf(stderr, "  run: exited after %d frames\n", frame_count);
}