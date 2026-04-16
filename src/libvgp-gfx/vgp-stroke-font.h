#ifndef VGP_STROKE_FONT_H
#define VGP_STROKE_FONT_H

/* Vector Stroke Font
 * Every glyph is a series of line segments on a 5x7 grid.
 * No font files. No TTF. No bitmap. Pure vector strokes.
 * Renders at any size by scaling the grid to pixel dimensions.
 *
 * Stroke data format: array of (x1,y1,x2,y2) pairs.
 * Coordinates are 0-5 horizontal, 0-7 vertical.
 * A segment with x1=255 marks end of glyph.
 * A segment with x1=254 is a "pen up" (move without drawing).
 */

#include <stdint.h>

#define STROKE_GRID_W  5
#define STROKE_GRID_H  7
#define STROKE_END     255
#define STROKE_MAX_SEG 24  /* max segments per glyph */

typedef struct {
    uint8_t x1, y1, x2, y2;
} stroke_seg_t;

typedef struct {
    stroke_seg_t segs[STROKE_MAX_SEG];
    uint8_t      width;  /* character width in grid units (typically 5) */
} stroke_glyph_t;

/* Get glyph data for an ASCII character (32-126) */
const stroke_glyph_t *stroke_font_glyph(int ch);

/* Get advance width for a character in grid units */
float stroke_font_advance(int ch);

/* Measure text width at a given size */
float stroke_font_text_width(const char *text, int len, float size);

#endif /* VGP_STROKE_FONT_H */
