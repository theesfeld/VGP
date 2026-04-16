/* VGP Edit -- GPU-rendered graphical text editor with syntax highlighting */

#include "vgp-gfx.h"

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

static bool is_keyword(const char *word, int len)
{
    static const char *kws[] = {
        "if","else","for","while","return","int","void","char","float","double",
        "struct","typedef","enum","const","static","bool","true","false",
        "break","continue","switch","case","default","sizeof","NULL",
        "include","define","ifdef","ifndef","endif",NULL
    };
    for (int i = 0; kws[i]; i++)
        if ((int)strlen(kws[i]) == len && strncmp(word, kws[i], (size_t)len) == 0) return true;
    return false;
}

static vgfx_color_t syntax_color(vgfx_ctx_t *ctx, char c, const char *line, int col)
{
    (void)col;
    /* Comment detection */
    if (line[0] == '/' && line[1] == '/') return vgfx_theme_color(ctx, VGP_THEME_FG_DISABLED);
    if (line[0] == '#') return vgfx_theme_color(ctx, VGP_THEME_WARNING);
    if (c == '"' || c == '\'') return vgfx_theme_color(ctx, VGP_THEME_SUCCESS);
    if (c >= '0' && c <= '9') return vgfx_theme_color(ctx, VGP_THEME_WARNING);
    return vgfx_theme_color(ctx, VGP_THEME_FG);
}

static void render(vgfx_ctx_t *ctx)
{
    vgfx_clear(ctx, vgfx_theme_color(ctx, VGP_THEME_BG));
    float p = ctx->theme.padding;
    float fs = ctx->theme.font_size;
    float lh = fs + 4; /* line height */
    float w = ctx->width, h = ctx->height;

    /* Title bar */
    vgfx_rounded_rect(ctx, 0, 0, w, 28, 0, vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));
    char title[256];
    snprintf(title, sizeof(title), "%s%s", ed.filename[0] ? ed.filename : "Untitled",
             ed.modified ? " *" : "");
    vgfx_text_bold(ctx, title, p, 19, fs, vgfx_theme_color(ctx, VGP_THEME_FG));

    /* Editor area */
    float gutter_w = 50;
    float ey = 32, ex = gutter_w, ew = w - gutter_w - p, eh = h - 32 - 24;
    int visible = (int)(eh / lh);

    /* Keyboard input */
    if (ctx->key_pressed) {
        if (ctx->last_keysym == 0xFF52 && ed.cursor_row > 0) ed.cursor_row--; /* Up */
        else if (ctx->last_keysym == 0xFF54 && ed.cursor_row < ed.line_count - 1) ed.cursor_row++; /* Down */
        else if (ctx->last_keysym == 0xFF51 && ed.cursor_col > 0) ed.cursor_col--; /* Left */
        else if (ctx->last_keysym == 0xFF53) ed.cursor_col++; /* Right */
        else if (ctx->last_keysym == 0xFF08) { /* Backspace */
            int len = (int)strlen(ed.lines[ed.cursor_row]);
            if (ed.cursor_col > 0 && ed.cursor_col <= len) {
                memmove(&ed.lines[ed.cursor_row][ed.cursor_col-1],
                        &ed.lines[ed.cursor_row][ed.cursor_col], (size_t)(len - ed.cursor_col + 1));
                ed.cursor_col--; ed.modified = true;
            } else if (ed.cursor_col == 0 && ed.cursor_row > 0) {
                int prev_len = (int)strlen(ed.lines[ed.cursor_row - 1]);
                strncat(ed.lines[ed.cursor_row - 1], ed.lines[ed.cursor_row],
                        MAX_LINE_LEN - prev_len - 1);
                memmove(&ed.lines[ed.cursor_row], &ed.lines[ed.cursor_row + 1],
                        (size_t)(ed.line_count - ed.cursor_row - 1) * MAX_LINE_LEN);
                ed.line_count--; ed.cursor_row--; ed.cursor_col = prev_len; ed.modified = true;
            }
        } else if (ctx->last_keysym == 0xFF0D) { /* Enter */
            if (ed.line_count < MAX_LINES) {
                memmove(&ed.lines[ed.cursor_row + 2], &ed.lines[ed.cursor_row + 1],
                        (size_t)(ed.line_count - ed.cursor_row - 1) * MAX_LINE_LEN);
                int len = (int)strlen(ed.lines[ed.cursor_row]);
                snprintf(ed.lines[ed.cursor_row + 1], MAX_LINE_LEN, "%s",
                         &ed.lines[ed.cursor_row][ed.cursor_col]);
                ed.lines[ed.cursor_row][ed.cursor_col] = '\0';
                ed.line_count++; ed.cursor_row++; ed.cursor_col = 0; ed.modified = true;
                (void)len;
            }
        } else if ((ctx->last_mods & 0x04) && (ctx->last_keysym == 's' || ctx->last_keysym == 'S')) {
            save_file(); /* Ctrl+S */
        } else if (ctx->last_utf8[0] >= 0x20) {
            int len = (int)strlen(ed.lines[ed.cursor_row]);
            if (len < MAX_LINE_LEN - 2) {
                memmove(&ed.lines[ed.cursor_row][ed.cursor_col + 1],
                        &ed.lines[ed.cursor_row][ed.cursor_col], (size_t)(len - ed.cursor_col + 1));
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

    /* Gutter (line numbers) */
    vgfx_rect(ctx, 0, ey, gutter_w - 4, eh, vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));

    vgfx_push_clip(ctx, 0, ey, w, eh);
    for (int i = ed.scroll; i < ed.line_count && i < ed.scroll + visible; i++) {
        float ly = ey + (float)(i - ed.scroll) * lh;

        /* Line number */
        char num[8]; snprintf(num, sizeof(num), "%4d", i + 1);
        vgfx_text(ctx, num, 4, ly + fs, fs - 2, vgfx_theme_color(ctx, VGP_THEME_FG_DISABLED));

        /* Current line highlight */
        if (i == ed.cursor_row)
            vgfx_rect(ctx, ex, ly, ew, lh, vgfx_alpha(vgfx_theme_color(ctx, VGP_THEME_ACCENT), 0.08f));

        /* Line text with basic syntax coloring */
        const char *line = ed.lines[i];
        float tx = ex + 4;
        float char_w = vgfx_text_width(ctx, "M", 1, fs);
        for (int c = 0; line[c]; c++) {
            char ch[2] = {line[c], '\0'};
            vgfx_color_t sc = syntax_color(ctx, line[c], line, c);
            vgfx_text(ctx, ch, tx + (float)c * char_w, ly + fs, fs, sc);
        }
    }

    /* Cursor */
    {
        float cy = ey + (float)(ed.cursor_row - ed.scroll) * lh;
        float char_w = vgfx_text_width(ctx, "M", 1, fs);
        float cx = ex + 4 + (float)ed.cursor_col * char_w;
        vgfx_rect(ctx, cx, cy + 2, 2, lh - 4, vgfx_theme_color(ctx, VGP_THEME_ACCENT));
    }
    vgfx_pop_clip(ctx);

    /* Status bar */
    vgfx_rect(ctx, 0, h - 22, w, 22, vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));
    char status[128];
    snprintf(status, sizeof(status), "Ln %d, Col %d | %d lines | Ctrl+S save | Esc close",
             ed.cursor_row + 1, ed.cursor_col + 1, ed.line_count);
    vgfx_text(ctx, status, p, h - 6, fs - 2, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));

    if (ctx->key_pressed && ctx->last_keysym == 0xFF1B) ctx->running = false;
}

int main(int argc, char *argv[])
{
    if (argc > 1) load_file(argv[1]);
    else { ed.lines[0][0] = '\0'; ed.line_count = 1; }

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Edit", 800, 600, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
