#include "term.h"

void vgp_term_palette_init(vgp_term_t *term)
{
    /* Standard ANSI colors (0-7) */
    static const uint32_t ansi_colors[16] = {
        0x000000, 0xCC0000, 0x4E9A06, 0xC4A000,
        0x3465A4, 0x75507B, 0x06989A, 0xD3D7CF,
        /* Bright (8-15) */
        0x555753, 0xEF2929, 0x8AE234, 0xFCE94F,
        0x729FCF, 0xAD7FA8, 0x34E2E2, 0xEEEEEC,
    };

    for (int i = 0; i < 16; i++) {
        term->palette[i] = (vgp_term_color_t){
            .r = ((ansi_colors[i] >> 16) & 0xFF) / 255.0f,
            .g = ((ansi_colors[i] >> 8) & 0xFF) / 255.0f,
            .b = (ansi_colors[i] & 0xFF) / 255.0f,
            .a = 1.0f,
        };
    }

    /* 6x6x6 color cube (16-231) */
    for (int i = 0; i < 216; i++) {
        int r = i / 36;
        int g = (i / 6) % 6;
        int b = i % 6;
        term->palette[16 + i] = (vgp_term_color_t){
            .r = r ? (r * 40 + 55) / 255.0f : 0,
            .g = g ? (g * 40 + 55) / 255.0f : 0,
            .b = b ? (b * 40 + 55) / 255.0f : 0,
            .a = 1.0f,
        };
    }

    /* Grayscale ramp (232-255) */
    for (int i = 0; i < 24; i++) {
        float v = (8 + i * 10) / 255.0f;
        term->palette[232 + i] = (vgp_term_color_t){ v, v, v, 1.0f };
    }

    /* Default fg and bg */
    term->palette[256] = (vgp_term_color_t){ 0.85f, 0.85f, 0.85f, 1.0f };
    term->palette[257] = (vgp_term_color_t){ 0.12f, 0.12f, 0.12f, 1.0f };
}
