/* VGP Edit -- HUD symbology text editor.
 * Line numbers as altitude tape marks, cursor as a crosshair, HUD-color
 * syntax (white / yellow / red). Status bar in MFD boxed-field format. */

#include "vgp-gfx.h"
#include "vgp-hud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINES 10000
#define MAX_LINE_LEN 512

static struct {
    char lines[MAX_LINES][MAX_LINE_LEN];
    int  line_count;
    int  cursor_row, cursor_col;
    int  scroll;
    char filename[512];
    bool modified;
} ed;

static void load_file(const char *path)
{
    snprintf(ed.filename, sizeof(ed.filename), "%s", path);
    ed.line_count = 0;
    FILE *f = fopen(path, "r");
    if (!f) { ed.lines[0][0] = '\0'; ed.line_count = 1; return; }
    while (ed.line_count < MAX_LINES && fgets(ed.lines[ed.line_count], MAX_LINE_LEN, f)) {
        size_t len = strlen(ed.lines[ed.line_count]);
        while (len > 0 && (ed.lines[ed.line_count][len-1] == '\n' ||
                            ed.lines[ed.line_count][len-1] == '\r'))
            ed.lines[ed.line_count][--len] = '\0';
        ed.line_count++;
    }
    fclose(f);
    if (ed.line_count == 0) { ed.lines[0][0] = '\0'; ed.line_count = 1; }
}

static void save_file(void)
{
    if (!ed.filename[0]) return;
    FILE *f = fopen(ed.filename, "w");
    if (!f) return;
    for (int i = 0; i < ed.line_count; i++)
        fprintf(f, "%s\n", ed.lines[i]);
    fclose(f);
    ed.modified = false;
}

static bool is_keyword_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

static bool word_is_keyword(const char *s, int len)
{
    static const char *kws[] = {
        "if","else","for","while","return","int","void","char","float","double",
        "struct","typedef","enum","const","static","bool","true","false",
        "break","continue","switch","case","default","sizeof","NULL",
        "include","define","ifdef","ifndef","endif",NULL
    };
    for (int i = 0; kws[i]; i++)
        if ((int)strlen(kws[i]) == len && strncmp(s, kws[i], (size_t)len) == 0)
            return true;
    return false;
}

static void render(vgfx_ctx_t *ctx)
{
    hud_palette_t P = hud_palette();
    vgfx_clear(ctx, vgfx_rgba(0, 0, 0, 0));

    /* OSBs: top = file ops, bottom = nav helpers */
    hud_osb_t top_osb[] = {
        { "SAVE",  false, ed.modified },
        { "SYNTX", false, false },
        { "FIND",  false, false },
        { "QUIT",  false, true },
    };
    hud_osb_t bot_osb[] = {
        { "TOP",  false, true },
        { "BOT",  false, true },
        { "GOTO", false, false },
        { "HELP", false, false },
    };

    /* Title shows filename (etched static) + modified flag (projected) */
    char title[600];
    snprintf(title, sizeof(title), "SYMB-EDIT  %s%s",
             ed.filename[0] ? ed.filename : "[new]",
             ed.modified ? "  [MOD]" : "");

    hud_mfd_t mfd = { 0 };
    mfd.top = top_osb;     mfd.top_count = 4;
    mfd.bottom = bot_osb;  mfd.bottom_count = 4;
    mfd.title = title;

    float cx, cy, cw, ch;
    hud_mfd_frame(ctx, &mfd, &P, &cx, &cy, &cw, &ch);

    /* OSB handling */
    if (mfd.clicked_edge == 1) {
        switch (mfd.clicked_index) {
        case 0: if (ed.modified) save_file(); break;
        case 3: ctx->running = false; return;
        }
    }
    if (mfd.clicked_edge == 3) {
        switch (mfd.clicked_index) {
        case 0: ed.cursor_row = 0; ed.cursor_col = 0; break;
        case 1: ed.cursor_row = ed.line_count - 1; ed.cursor_col = 0; break;
        }
    }

    /* Editor area */
    float fs = 14.0f;
    float lh = fs + 4.0f;
    float gutter_w = 48.0f;
    float ex = cx + gutter_w, ey = cy, ew = cw - gutter_w, eh = ch - 30.0f;
    int visible = (int)(eh / lh);
    if (visible < 1) visible = 1;

    /* Keyboard input */
    if (ctx->key_pressed) {
        if (ctx->last_keysym == 0xFF52 && ed.cursor_row > 0) ed.cursor_row--;
        else if (ctx->last_keysym == 0xFF54 && ed.cursor_row < ed.line_count - 1) ed.cursor_row++;
        else if (ctx->last_keysym == 0xFF51 && ed.cursor_col > 0) ed.cursor_col--;
        else if (ctx->last_keysym == 0xFF53) ed.cursor_col++;
        else if (ctx->last_keysym == 0xFF08) {
            int len = (int)strlen(ed.lines[ed.cursor_row]);
            if (ed.cursor_col > 0 && ed.cursor_col <= len) {
                memmove(&ed.lines[ed.cursor_row][ed.cursor_col-1],
                        &ed.lines[ed.cursor_row][ed.cursor_col],
                        (size_t)(len - ed.cursor_col + 1));
                ed.cursor_col--; ed.modified = true;
            } else if (ed.cursor_col == 0 && ed.cursor_row > 0) {
                int prev_len = (int)strlen(ed.lines[ed.cursor_row - 1]);
                strncat(ed.lines[ed.cursor_row - 1], ed.lines[ed.cursor_row],
                        MAX_LINE_LEN - prev_len - 1);
                memmove(&ed.lines[ed.cursor_row], &ed.lines[ed.cursor_row + 1],
                        (size_t)(ed.line_count - ed.cursor_row - 1) * MAX_LINE_LEN);
                ed.line_count--; ed.cursor_row--;
                ed.cursor_col = prev_len; ed.modified = true;
            }
        } else if (ctx->last_keysym == 0xFF0D) {
            if (ed.line_count < MAX_LINES) {
                memmove(&ed.lines[ed.cursor_row + 2], &ed.lines[ed.cursor_row + 1],
                        (size_t)(ed.line_count - ed.cursor_row - 1) * MAX_LINE_LEN);
                snprintf(ed.lines[ed.cursor_row + 1], MAX_LINE_LEN, "%s",
                         &ed.lines[ed.cursor_row][ed.cursor_col]);
                ed.lines[ed.cursor_row][ed.cursor_col] = '\0';
                ed.line_count++; ed.cursor_row++; ed.cursor_col = 0;
                ed.modified = true;
            }
        } else if ((ctx->last_mods & 0x04) &&
                    (ctx->last_keysym == 's' || ctx->last_keysym == 'S')) {
            save_file();
        } else if (ctx->last_keysym == 0xFF1B) {
            ctx->running = false;
            return;
        } else if (ctx->last_utf8[0] >= 0x20) {
            int len = (int)strlen(ed.lines[ed.cursor_row]);
            if (len < MAX_LINE_LEN - 2) {
                memmove(&ed.lines[ed.cursor_row][ed.cursor_col + 1],
                        &ed.lines[ed.cursor_row][ed.cursor_col],
                        (size_t)(len - ed.cursor_col + 1));
                ed.lines[ed.cursor_row][ed.cursor_col] = ctx->last_utf8[0];
                ed.cursor_col++; ed.modified = true;
            }
        }
    }

    /* Clamp cursor */
    if (ed.cursor_col > (int)strlen(ed.lines[ed.cursor_row]))
        ed.cursor_col = (int)strlen(ed.lines[ed.cursor_row]);
    if (ed.cursor_row < ed.scroll) ed.scroll = ed.cursor_row;
    if (ed.cursor_row >= ed.scroll + visible) ed.scroll = ed.cursor_row - visible + 1;

    /* Gutter -- altitude tape with line numbers. Current line is yellow. */
    vgfx_rect(ctx, cx, ey, gutter_w, eh, P.shade);
    int highlight = ed.cursor_row - ed.scroll;
    hud_altitude_tape(ctx, cx, ey, gutter_w, eh,
                       ed.scroll + 1, 1, lh, highlight, &P);

    /* Editor body */
    vgfx_push_clip(ctx, ex, ey, ew, eh);
    for (int i = ed.scroll; i < ed.line_count && i < ed.scroll + visible; i++) {
        float ly = ey + (float)(i - ed.scroll) * lh;

        /* Current line: faint boxed highlight */
        if (i == ed.cursor_row) {
            vgfx_rect(ctx, ex, ly, ew, lh, vgfx_alpha(P.warn, 0.08f));
            vgfx_line(ctx, ex, ly, ex, ly + lh, 1.0f, P.warn);
        }

        const char *line = ed.lines[i];
        float tx = ex + 6.0f;
        float char_w = vgfx_text_width(ctx, "M", 1, fs);

        /* Comment / preprocessor whole-line rules */
        bool is_comment = (line[0] == '/' && line[1] == '/') ||
                           (line[0] == '-' && line[1] == '-');
        bool is_pp = line[0] == '#';

        int c = 0;
        while (line[c]) {
            /* String literal */
            if (!is_comment && !is_pp && (line[c] == '"' || line[c] == '\'')) {
                char quote = line[c];
                int start = c;
                c++;
                while (line[c] && line[c] != quote) {
                    if (line[c] == '\\' && line[c + 1]) c++;
                    c++;
                }
                if (line[c]) c++;
                char buf[256];
                int len = c - start;
                if (len > 255) len = 255;
                memcpy(buf, line + start, (size_t)len); buf[len] = '\0';
                vgfx_text(ctx, buf, tx + (float)start * char_w,
                           ly + fs, fs, P.warn);
                continue;
            }
            /* Word (possibly keyword) */
            if (is_keyword_char(line[c]) && !is_comment && !is_pp) {
                int start = c;
                while (line[c] && is_keyword_char(line[c])) c++;
                int len = c - start;
                char buf[64];
                if (len > 63) len = 63;
                memcpy(buf, line + start, (size_t)len); buf[len] = '\0';
                vgfx_color_t col = P.fg;
                if (buf[0] >= '0' && buf[0] <= '9') col = P.warn;
                else if (word_is_keyword(buf, len)) col = P.hi;
                vgfx_text(ctx, buf, tx + (float)start * char_w,
                           ly + fs, fs, col);
                continue;
            }
            /* Single char (operator / space / punctuation) */
            char ch[2] = { line[c], '\0' };
            vgfx_color_t col = is_comment ? P.dim :
                                (is_pp ? P.warn : P.fg);
            vgfx_text(ctx, ch, tx + (float)c * char_w, ly + fs, fs, col);
            c++;
        }
    }

    /* Crosshair cursor */
    {
        float cy2 = ey + (float)(ed.cursor_row - ed.scroll) * lh;
        float char_w = vgfx_text_width(ctx, "M", 1, fs);
        float cxp = ex + 6.0f + (float)ed.cursor_col * char_w;
        /* Vertical bar */
        vgfx_rect(ctx, cxp, cy2 + 2, 2, lh - 4, P.warn);
        /* Tick marks */
        vgfx_line(ctx, cxp - 4, cy2 + lh * 0.5f,
                         cxp - 1, cy2 + lh * 0.5f, 1.0f, P.warn);
        vgfx_line(ctx, cxp + 3, cy2 + lh * 0.5f,
                         cxp + 6, cy2 + lh * 0.5f, 1.0f, P.warn);
    }
    vgfx_pop_clip(ctx);

    /* Status bar: three boxed fields (ROW / COL / TOTAL) */
    float sy = ey + eh + 4.0f;
    char rbuf[16], cbuf[16], lbuf[16];
    snprintf(rbuf, sizeof(rbuf), "%d", ed.cursor_row + 1);
    snprintf(cbuf, sizeof(cbuf), "%d", ed.cursor_col + 1);
    snprintf(lbuf, sizeof(lbuf), "%d", ed.line_count);
    hud_boxed_field(ctx, cx,       sy, 90,  12, "ROW",   rbuf, P.warn, &P);
    hud_boxed_field(ctx, cx + 96,  sy, 90,  12, "COL",   cbuf, P.warn, &P);
    hud_boxed_field(ctx, cx + 192, sy, 110, 12, "LINES", lbuf, P.fg,   &P);
}

int main(int argc, char *argv[])
{
    if (argc > 1) load_file(argv[1]);
    else { ed.lines[0][0] = '\0'; ed.line_count = 1; }

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Edit", 840, 600, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
