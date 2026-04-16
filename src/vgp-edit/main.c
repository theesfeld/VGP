/* VGP Edit -- Simple text editor with syntax highlighting
 * Vector-rendered via cell grid protocol. */

#include "vgp-ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINES 10000
#define MAX_LINE_LEN 1024

typedef struct {
    char  lines[MAX_LINES][MAX_LINE_LEN];
    int   line_count;
    int   cursor_row, cursor_col;
    int   scroll_row;
    char  filename[512];
    bool  modified;
    char  status[128];
    int   status_timer;
} editor_state_t;

static editor_state_t ed;

static void load_file(const char *path)
{
    snprintf(ed.filename, sizeof(ed.filename), "%s", path);
    ed.line_count = 0;
    ed.cursor_row = 0;
    ed.cursor_col = 0;
    ed.scroll_row = 0;
    ed.modified = false;

    FILE *f = fopen(path, "r");
    if (!f) {
        /* New file */
        ed.line_count = 1;
        ed.lines[0][0] = '\0';
        return;
    }

    while (ed.line_count < MAX_LINES &&
           fgets(ed.lines[ed.line_count], MAX_LINE_LEN, f)) {
        /* Strip newline */
        size_t len = strlen(ed.lines[ed.line_count]);
        while (len > 0 && (ed.lines[ed.line_count][len-1] == '\n' ||
                            ed.lines[ed.line_count][len-1] == '\r'))
            ed.lines[ed.line_count][--len] = '\0';
        ed.line_count++;
    }
    fclose(f);
    if (ed.line_count == 0) {
        ed.line_count = 1;
        ed.lines[0][0] = '\0';
    }
}

static void save_file(void)
{
    FILE *f = fopen(ed.filename, "w");
    if (!f) {
        snprintf(ed.status, sizeof(ed.status), "Error: cannot save %s", ed.filename);
        ed.status_timer = 120;
        return;
    }
    for (int i = 0; i < ed.line_count; i++)
        fprintf(f, "%s\n", ed.lines[i]);
    fclose(f);
    ed.modified = false;
    snprintf(ed.status, sizeof(ed.status), "Saved: %s (%d lines)", ed.filename, ed.line_count);
    ed.status_timer = 120;
}

/* Simple keyword detection for syntax highlighting */
static bool is_keyword(const char *word, int len)
{
    static const char *keywords[] = {
        "if","else","for","while","return","int","char","void","float","double",
        "struct","typedef","enum","const","static","bool","true","false","NULL",
        "include","define","ifdef","endif","ifndef","break","continue","switch",
        "case","default","do","sizeof","unsigned","long","short","extern",
        "import","from","def","class","self","print","fn","let","mut","pub",
        "use","mod","crate","async","await","function","var","const",NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if ((int)strlen(keywords[i]) == len && strncmp(word, keywords[i], (size_t)len) == 0)
            return true;
    }
    return false;
}

static vui_color_t syntax_color(char c, const char *line, int col)
{
    /* Comment detection */
    if (col >= 1 && line[col-1] == '/' && c == '/') return (vui_color_t){0x60, 0x70, 0x60};
    if (c == '/' && col + 1 < (int)strlen(line) && line[col+1] == '/') return (vui_color_t){0x60, 0x70, 0x60};

    /* String */
    if (c == '"' || c == '\'') return (vui_color_t){0xC0, 0x90, 0x50};

    /* Number */
    if (c >= '0' && c <= '9') return (vui_color_t){0xB0, 0x80, 0xD0};

    /* Preprocessor */
    if (c == '#') return (vui_color_t){0x90, 0xB0, 0xC0};

    /* Brackets */
    if (c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']')
        return (vui_color_t){0xE0, 0xC0, 0x40};

    return VUI_WHITE;
}

static void render(vui_ctx_t *ctx)
{
    vui_clear(ctx, VUI_BG);

    /* Title bar */
    vui_fill(ctx, 0, 0, 1, ctx->cols, VUI_SURFACE);
    char title[256];
    snprintf(title, sizeof(title), " VGP Edit - %s%s ",
             ed.filename[0] ? ed.filename : "[new]",
             ed.modified ? " [modified]" : "");
    vui_text_bold(ctx, 0, 2, title, VUI_ACCENT, VUI_SURFACE);

    /* Line number width */
    int ln_width = 5;
    int text_start = ln_width + 1;
    int visible_lines = ctx->rows - 3;

    /* Handle keyboard */
    if (ctx->key_pressed) {
        uint32_t sym = ctx->last_keysym;
        bool ctrl = (ctx->last_mods & 0x0002) != 0;

        if (ctrl && (sym == 0x0073 || sym == 0x0053)) { /* Ctrl+S */
            save_file();
        } else if (sym == 0xFF52) { /* Up */
            if (ed.cursor_row > 0) ed.cursor_row--;
        } else if (sym == 0xFF54) { /* Down */
            if (ed.cursor_row < ed.line_count - 1) ed.cursor_row++;
        } else if (sym == 0xFF51) { /* Left */
            if (ed.cursor_col > 0) ed.cursor_col--;
        } else if (sym == 0xFF53) { /* Right */
            ed.cursor_col++;
        } else if (sym == 0xFF50) { /* Home */
            ed.cursor_col = 0;
        } else if (sym == 0xFF57) { /* End */
            ed.cursor_col = (int)strlen(ed.lines[ed.cursor_row]);
        } else if (sym == 0xFF0D) { /* Enter */
            if (ed.line_count < MAX_LINES) {
                memmove(&ed.lines[ed.cursor_row + 2], &ed.lines[ed.cursor_row + 1],
                        (size_t)(ed.line_count - ed.cursor_row - 1) * MAX_LINE_LEN);
                char *cur = ed.lines[ed.cursor_row];
                int len = (int)strlen(cur);
                if (ed.cursor_col < len) {
                    snprintf(ed.lines[ed.cursor_row + 1], MAX_LINE_LEN, "%s", cur + ed.cursor_col);
                    cur[ed.cursor_col] = '\0';
                } else {
                    ed.lines[ed.cursor_row + 1][0] = '\0';
                }
                ed.line_count++;
                ed.cursor_row++;
                ed.cursor_col = 0;
                ed.modified = true;
            }
        } else if (sym == 0xFF08) { /* Backspace */
            if (ed.cursor_col > 0) {
                char *line = ed.lines[ed.cursor_row];
                int len = (int)strlen(line);
                memmove(line + ed.cursor_col - 1, line + ed.cursor_col, (size_t)(len - ed.cursor_col + 1));
                ed.cursor_col--;
                ed.modified = true;
            } else if (ed.cursor_row > 0) {
                int prev_len = (int)strlen(ed.lines[ed.cursor_row - 1]);
                strncat(ed.lines[ed.cursor_row - 1], ed.lines[ed.cursor_row],
                        MAX_LINE_LEN - prev_len - 1);
                memmove(&ed.lines[ed.cursor_row], &ed.lines[ed.cursor_row + 1],
                        (size_t)(ed.line_count - ed.cursor_row - 1) * MAX_LINE_LEN);
                ed.line_count--;
                ed.cursor_row--;
                ed.cursor_col = prev_len;
                ed.modified = true;
            }
        } else if (sym == 0xFF1B) { /* Escape */
            ctx->running = false;
        } else if (ctx->last_utf8[0] >= 0x20 && !ctrl) {
            char *line = ed.lines[ed.cursor_row];
            int len = (int)strlen(line);
            if (len < MAX_LINE_LEN - 2) {
                memmove(line + ed.cursor_col + 1, line + ed.cursor_col, (size_t)(len - ed.cursor_col + 1));
                line[ed.cursor_col] = ctx->last_utf8[0];
                ed.cursor_col++;
                ed.modified = true;
            }
        }

        /* Clamp cursor */
        if (ed.cursor_col > (int)strlen(ed.lines[ed.cursor_row]))
            ed.cursor_col = (int)strlen(ed.lines[ed.cursor_row]);

        /* Scroll to keep cursor visible */
        if (ed.cursor_row < ed.scroll_row) ed.scroll_row = ed.cursor_row;
        if (ed.cursor_row >= ed.scroll_row + visible_lines)
            ed.scroll_row = ed.cursor_row - visible_lines + 1;
    }

    /* Render lines */
    for (int i = 0; i < visible_lines && ed.scroll_row + i < ed.line_count; i++) {
        int line_idx = ed.scroll_row + i;
        int row = i + 2;

        /* Line number */
        char ln[8];
        snprintf(ln, sizeof(ln), "%4d", line_idx + 1);
        vui_text(ctx, row, 0, ln, VUI_GRAY, VUI_BG);
        vui_set_cell(ctx, row, ln_width, 0x2502, VUI_BORDER, VUI_BG, 0); /* │ */

        /* Text with syntax coloring */
        char *line = ed.lines[line_idx];
        int len = (int)strlen(line);
        bool in_comment = false;
        for (int c = 0; c < len && text_start + c < ctx->cols; c++) {
            if (c >= 1 && line[c-1] == '/' && line[c] == '/')
                in_comment = true;
            vui_color_t fg = in_comment ? (vui_color_t){0x60,0x70,0x60} : syntax_color(line[c], line, c);
            vui_set_cell(ctx, row, text_start + c, (uint32_t)(unsigned char)line[c], fg, VUI_BG, 0);
        }

        /* Cursor */
        if (line_idx == ed.cursor_row) {
            int cc = text_start + ed.cursor_col;
            if (cc < ctx->cols)
                vui_set_cell(ctx, row, cc, ed.cursor_col < len ? (uint32_t)(unsigned char)line[ed.cursor_col] : ' ',
                              VUI_BG, VUI_WHITE, 0);
        }
    }

    /* Status bar */
    vui_fill(ctx, ctx->rows - 1, 0, 1, ctx->cols, VUI_SURFACE);
    if (ed.status_timer > 0) {
        vui_text(ctx, ctx->rows - 1, 2, ed.status, VUI_GREEN, VUI_SURFACE);
        ed.status_timer--;
    } else {
        char st[128];
        snprintf(st, sizeof(st), " Ln %d, Col %d | %d lines | Ctrl+S: save | Esc: quit",
                 ed.cursor_row + 1, ed.cursor_col + 1, ed.line_count);
        vui_text(ctx, ctx->rows - 1, 1, st, VUI_GRAY, VUI_SURFACE);
    }
}

int main(int argc, char *argv[])
{
    FILE *lf = fopen("/tmp/vgp-edit.log", "w");
    if (lf) { setvbuf(lf, NULL, _IOLBF, 0); dup2(fileno(lf), STDERR_FILENO); fclose(lf); }

    memset(&ed, 0, sizeof(ed));
    if (argc > 1)
        load_file(argv[1]);
    else {
        ed.line_count = 1;
        ed.lines[0][0] = '\0';
    }

    vui_ctx_t ctx;
    if (vui_init(&ctx, "VGP Edit", 800, 600) < 0) return 1;
    vui_run(&ctx, render);
    vui_destroy(&ctx);
    return 0;
}
