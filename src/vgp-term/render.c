#include "term.h"
#include "vgp/protocol.h"

#include <string.h>

/* ============================================================
 * Font metrics (still needed for computing grid dimensions)
 * ============================================================ */

int vgp_term_fonts_init(vgp_term_fonts_t *fonts, float size)
{
    /* We no longer render locally, but we need cell metrics to compute
     * the grid size from pixel dimensions. Use hardcoded monospace metrics
     * matching common fonts at the given size. The server handles actual
     * font rendering. */
    memset(fonts, 0, sizeof(*fonts));
    fonts->size = size;

    /* Approximate cell dimensions for a monospace font.
     * The server will use its own font metrics for actual rendering.
     * These values match DejaVu Sans Mono at 14pt reasonably well. */
    fonts->cell_width = size * 0.6f;
    fonts->cell_height = size * 1.3f + 2.0f;
    fonts->ascent = size * 0.9f;

    return 0;
}

void vgp_term_fonts_destroy(vgp_term_fonts_t *fonts)
{
    (void)fonts;
    /* No local font resources to free -- server owns the fonts */
}

/* ============================================================
 * Cell grid rendering (vector protocol)
 * ============================================================ */

/* Resolve a VTermColor to RGB bytes */
static void resolve_color(vgp_term_t *term, const VTermColor *vc,
                           uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(vc)) {
        *r = (uint8_t)(term->palette[256].r * 255);
        *g = (uint8_t)(term->palette[256].g * 255);
        *b = (uint8_t)(term->palette[256].b * 255);
        return;
    }
    if (VTERM_COLOR_IS_DEFAULT_BG(vc)) {
        *r = (uint8_t)(term->palette[257].r * 255);
        *g = (uint8_t)(term->palette[257].g * 255);
        *b = (uint8_t)(term->palette[257].b * 255);
        return;
    }
    if (VTERM_COLOR_IS_INDEXED(vc)) {
        *r = (uint8_t)(term->palette[vc->indexed.idx].r * 255);
        *g = (uint8_t)(term->palette[vc->indexed.idx].g * 255);
        *b = (uint8_t)(term->palette[vc->indexed.idx].b * 255);
        return;
    }
    /* True color */
    *r = vc->rgb.red;
    *g = vc->rgb.green;
    *b = vc->rgb.blue;
}

void vgp_term_render_dirty(vgp_term_t *term)
{
    /* Nothing to render locally -- we send cell data to the server */
    (void)term;
}

void vgp_term_render_row(vgp_term_t *term, int row)
{
    (void)term; (void)row;
}

void vgp_term_render_cursor(vgp_term_t *term)
{
    (void)term;
}

void vgp_term_commit_surface(vgp_term_t *term)
{
    if (!term->conn || !term->window_id) return;

    /* Build the cell grid from libvterm's screen state */
    int rows = term->rows;
    int cols = term->cols;
    size_t grid_size = (size_t)rows * (size_t)cols * sizeof(vgp_cell_t);
    vgp_cell_t *grid = malloc(grid_size);
    if (!grid) return;
    memset(grid, 0, grid_size);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; ) {
            VTermPos pos = { .row = row, .col = col };
            VTermScreenCell cell;
            vterm_screen_get_cell(term->vt_screen, pos, &cell);

            vgp_cell_t *c = &grid[row * cols + col];

            /* Codepoint */
            c->codepoint = cell.chars[0];
            c->width = cell.width > 0 ? (uint8_t)cell.width : 1;

            /* Colors */
            resolve_color(term, &cell.fg, &c->fg_r, &c->fg_g, &c->fg_b);
            resolve_color(term, &cell.bg, &c->bg_r, &c->bg_g, &c->bg_b);

            /* Attributes */
            c->attrs = 0;
            if (cell.attrs.bold)      c->attrs |= VGP_CELL_BOLD;
            if (cell.attrs.italic)    c->attrs |= VGP_CELL_ITALIC;
            if (cell.attrs.underline) c->attrs |= VGP_CELL_UNDERLINE;
            if (cell.attrs.strike)    c->attrs |= VGP_CELL_STRIKE;
            if (cell.attrs.reverse)   c->attrs |= VGP_CELL_REVERSE;
            if (cell.attrs.blink)     c->attrs |= VGP_CELL_BLINK;

            col += c->width;
        }
    }

    /* URL detection: scan for http:// https:// and underline them */
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols - 7; col++) {
            /* Check for URL prefix */
            bool is_url = false;
            if (grid[row * cols + col].codepoint == 'h' &&
                grid[row * cols + col + 1].codepoint == 't' &&
                grid[row * cols + col + 2].codepoint == 't' &&
                grid[row * cols + col + 3].codepoint == 'p') {
                if (grid[row * cols + col + 4].codepoint == ':')
                    is_url = true;
                else if (grid[row * cols + col + 4].codepoint == 's' &&
                         col + 5 < cols && grid[row * cols + col + 5].codepoint == ':')
                    is_url = true;
            }
            if (is_url) {
                /* Mark the entire URL with underline */
                for (int c = col; c < cols; c++) {
                    uint32_t cp = grid[row * cols + c].codepoint;
                    if (cp <= 32 || cp == '"' || cp == '\'' || cp == '>' || cp == ')') break;
                    grid[row * cols + c].attrs |= VGP_CELL_UNDERLINE;
                    grid[row * cols + c].fg_r = 0x52;
                    grid[row * cols + c].fg_g = 0x94;
                    grid[row * cols + c].fg_b = 0xE2;
                }
            }
        }
    }

    /* Highlight search matches */
    if (term->search.active && term->search.query_len > 0) {
        for (int row = 0; row < rows; row++) {
            for (int col = 0; col < cols - term->search.query_len; col++) {
                bool match = true;
                for (int q = 0; q < term->search.query_len && match; q++) {
                    if ((char)grid[row * cols + col + q].codepoint != term->search.query[q])
                        match = false;
                }
                if (match) {
                    for (int q = 0; q < term->search.query_len; q++) {
                        vgp_cell_t *c = &grid[row * cols + col + q];
                        c->bg_r = 0xE0; c->bg_g = 0xC0; c->bg_b = 0x40;
                        c->fg_r = 0x10; c->fg_g = 0x10; c->fg_b = 0x10;
                    }
                }
            }
        }
    }

    /* Send cell grid to server */
    vgp_cellgrid_send(term->conn, term->window_id,
                       (uint16_t)rows, (uint16_t)cols,
                       (uint16_t)term->cursor.pos.row,
                       (uint16_t)term->cursor.pos.col,
                       term->cursor.visible ? 1 : 0,
                       (uint8_t)term->cursor.shape,
                       grid);
    free(grid);
}
