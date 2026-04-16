#ifndef VGP_PROTOCOL_H
#define VGP_PROTOCOL_H

#include <stdint.h>

#define VGP_PROTOCOL_MAGIC   0x56475000  /* 'VGP\0' */
#define VGP_PROTOCOL_VERSION ((0 << 16) | 1)  /* 0.1 */

/* ============================================================
 * Message header (16 bytes)
 * ============================================================ */

typedef struct vgp_msg_header {
    uint32_t magic;
    uint16_t type;
    uint16_t flags;
    uint32_t length;     /* total message length including header */
    uint32_t window_id;  /* 0 for connection-level messages */
} __attribute__((packed)) vgp_msg_header_t;

#define VGP_MSG_FLAG_NONE     0x0000
#define VGP_MSG_FLAG_BATCH    0x0001
#define VGP_MSG_FLAG_RESPONSE 0x0004
#define VGP_MSG_FLAG_ERROR    0x0008

/* ============================================================
 * Message types
 * ============================================================ */

typedef enum {
    /* Connection lifecycle */
    VGP_MSG_CONNECT          = 0x0001,
    VGP_MSG_CONNECT_OK       = 0x0002,
    VGP_MSG_DISCONNECT       = 0x0003,
    VGP_MSG_PING             = 0x0004,
    VGP_MSG_PONG             = 0x0005,
    VGP_MSG_ERROR            = 0x000F,

    /* Window lifecycle */
    VGP_MSG_WINDOW_CREATE    = 0x0010,
    VGP_MSG_WINDOW_CREATED   = 0x0011,
    VGP_MSG_WINDOW_DESTROY   = 0x0012,
    VGP_MSG_WINDOW_SET_TITLE = 0x0016,
    VGP_MSG_WINDOW_CONFIGURE = 0x001B,
    VGP_MSG_WINDOW_CLOSE     = 0x001C,

    /* Surface updates (pixel-based, legacy) */
    VGP_MSG_SURFACE_ATTACH   = 0x0070,
    VGP_MSG_SURFACE_COMMIT   = 0x0071,
    VGP_MSG_SURFACE_DAMAGE   = 0x0072,

    /* Terminal cell grid (vector text protocol) */
    VGP_MSG_CELLGRID         = 0x0090,
    VGP_MSG_SET_FONT_SIZE    = 0x0091,  /* client -> server: request font size change */

    /* Drawing commands (server-side rendering) */
    VGP_MSG_DRAW_BEGIN       = 0x0030,
    VGP_MSG_DRAW_END         = 0x0031,
    VGP_MSG_DRAW_CLEAR       = 0x0032,
    VGP_MSG_DRAW_COMMANDS    = 0x0080,

    /* Input events (server -> client) */
    VGP_MSG_KEY_PRESS        = 0x0100,
    VGP_MSG_KEY_RELEASE      = 0x0101,
    VGP_MSG_MOUSE_MOVE       = 0x0110,
    VGP_MSG_MOUSE_BUTTON     = 0x0111,
    VGP_MSG_MOUSE_SCROLL     = 0x0112,
    VGP_MSG_FOCUS_IN         = 0x0120,
    VGP_MSG_FOCUS_OUT        = 0x0121,

    /* Theme info (server -> client on connect + reload) */
    VGP_MSG_THEME_INFO       = 0x0033,

    /* Clipboard */
    VGP_MSG_CLIPBOARD_SET    = 0x01A0,  /* client -> server: set clipboard content */
    VGP_MSG_CLIPBOARD_GET    = 0x01A1,  /* client -> server: request clipboard */
    VGP_MSG_CLIPBOARD_DATA   = 0x01A2,  /* server -> client: clipboard content */
    VGP_MSG_OPEN_URL         = 0x01A3,  /* client -> server: open URL via handler */
} vgp_msg_type_t;

/* ============================================================
 * Message payloads
 * ============================================================ */

typedef struct vgp_msg_connect {
    vgp_msg_header_t header;
    uint32_t protocol_version;
    uint32_t client_pid;
    char     app_id[64];
} __attribute__((packed)) vgp_msg_connect_t;

typedef struct vgp_msg_connect_ok {
    vgp_msg_header_t header;
    uint32_t protocol_version;
    uint32_t client_id;
    uint32_t display_width;
    uint32_t display_height;
} __attribute__((packed)) vgp_msg_connect_ok_t;

typedef struct vgp_msg_window_create {
    vgp_msg_header_t header;
    int32_t  x, y;
    uint32_t width, height;
    uint32_t flags;
    uint32_t title_len;
    /* char title[] follows */
} __attribute__((packed)) vgp_msg_window_create_t;

typedef struct vgp_msg_window_created {
    vgp_msg_header_t header;
    uint32_t window_id;
    int32_t  x, y;
    uint32_t width, height;
} __attribute__((packed)) vgp_msg_window_created_t;

typedef struct vgp_msg_window_configure {
    vgp_msg_header_t header;
    int32_t  x, y;
    uint32_t width, height;
} __attribute__((packed)) vgp_msg_window_configure_t;

typedef struct vgp_msg_surface_attach {
    vgp_msg_header_t header;
    uint32_t width, height;
    uint32_t stride;
    uint32_t format;      /* 0 = ARGB8888 */
    /* pixel data follows: stride * height bytes */
} __attribute__((packed)) vgp_msg_surface_attach_t;

typedef struct vgp_msg_key_event {
    vgp_msg_header_t header;
    uint32_t keycode;
    uint32_t keysym;
    uint32_t modifiers;
    uint32_t utf8_len;
    char     utf8[8];
} __attribute__((packed)) vgp_msg_key_event_t;

typedef struct vgp_msg_mouse_move_event {
    vgp_msg_header_t header;
    float x, y;
    uint32_t modifiers;
} __attribute__((packed)) vgp_msg_mouse_move_event_t;

typedef struct vgp_msg_mouse_button_event {
    vgp_msg_header_t header;
    float    x, y;
    uint32_t button;
    uint32_t state;   /* 0=released, 1=pressed */
    uint32_t modifiers;
} __attribute__((packed)) vgp_msg_mouse_button_event_t;

typedef struct vgp_msg_mouse_scroll_event {
    vgp_msg_header_t header;
    float dx, dy;
    uint32_t modifiers;
} __attribute__((packed)) vgp_msg_mouse_scroll_event_t;

/* ============================================================
 * Cell grid (vector terminal protocol)
 * ============================================================ */

/* Per-cell attributes packed into a single byte */
#define VGP_CELL_BOLD      0x01
#define VGP_CELL_ITALIC    0x02
#define VGP_CELL_UNDERLINE 0x04
#define VGP_CELL_STRIKE    0x08
#define VGP_CELL_REVERSE   0x10
#define VGP_CELL_BLINK     0x20

/* A single terminal cell (12 bytes) */
typedef struct vgp_cell {
    uint32_t codepoint;   /* Unicode codepoint (0 = empty) */
    uint8_t  fg_r, fg_g, fg_b;
    uint8_t  bg_r, bg_g, bg_b;
    uint8_t  attrs;       /* VGP_CELL_* flags */
    uint8_t  width;       /* cell width (1 = normal, 2 = wide char) */
} __attribute__((packed)) vgp_cell_t;

/* Cell grid message: complete terminal state */
typedef struct vgp_msg_cellgrid {
    vgp_msg_header_t header;  /* type = VGP_MSG_CELLGRID */
    uint16_t rows;
    uint16_t cols;
    uint16_t cursor_row;
    uint16_t cursor_col;
    uint8_t  cursor_visible;
    uint8_t  cursor_shape;    /* 1=block, 2=underline, 3=bar */
    uint8_t  _pad[2];
    /* followed by: rows * cols * sizeof(vgp_cell_t) bytes */
} __attribute__((packed)) vgp_msg_cellgrid_t;

typedef struct vgp_msg_set_font_size {
    vgp_msg_header_t header;
    float            font_size;
} __attribute__((packed)) vgp_msg_set_font_size_t;

/* ============================================================
 * Draw commands (graphical UI protocol)
 * ============================================================
 * VGP_MSG_DRAW_COMMANDS carries a packed stream of draw opcodes.
 * All coordinates are window-local (0,0 = content area top-left).
 * Server translates to screen space during rendering.
 * Wire format: [u8 opcode][payload bytes] packed tightly.
 */

typedef enum {
    VGP_DCMD_CLEAR         = 0x01,  /* r,g,b,a (4f = 16B) */
    VGP_DCMD_RECT          = 0x02,  /* x,y,w,h,r,g,b,a (8f = 32B) */
    VGP_DCMD_ROUNDED_RECT  = 0x03,  /* x,y,w,h,radius,r,g,b,a (9f = 36B) */
    VGP_DCMD_CIRCLE        = 0x04,  /* cx,cy,rad,r,g,b,a (7f = 28B) */
    VGP_DCMD_LINE          = 0x05,  /* x1,y1,x2,y2,width,r,g,b,a (9f = 36B) */
    VGP_DCMD_TEXT          = 0x06,  /* x,y,size,r,g,b,a (7f=28B) + u16 len + text */
    VGP_DCMD_PUSH_STATE    = 0x07,  /* 0B payload */
    VGP_DCMD_POP_STATE     = 0x08,  /* 0B payload */
    VGP_DCMD_SET_CLIP      = 0x09,  /* x,y,w,h (4f = 16B) */
    VGP_DCMD_RECT_OUTLINE  = 0x0A,  /* x,y,w,h,line_w,r,g,b,a (9f = 36B) */
    VGP_DCMD_RRECT_OUTLINE = 0x0B,  /* x,y,w,h,radius,line_w,r,g,b,a (10f = 40B) */
    VGP_DCMD_TEXT_BOLD     = 0x0C,  /* same as TEXT */
    VGP_DCMD_GRADIENT_RECT = 0x0E,  /* x,y,w,h,r1,g1,b1,a1,r2,g2,b2,a2 (12f = 48B) */
    VGP_DCMD_TRANSFORM     = 0x0F,  /* 2D affine: a,b,c,d,e,f (6f = 24B) */
} vgp_dcmd_opcode_t;

/* Draw commands message header */
typedef struct vgp_msg_draw_commands {
    vgp_msg_header_t header;  /* type = VGP_MSG_DRAW_COMMANDS */
    uint32_t cmd_count;       /* number of commands in stream */
    uint32_t cmd_bytes;       /* total bytes of command data */
    /* command stream follows: cmd_bytes bytes of packed [opcode][payload] */
} __attribute__((packed)) vgp_msg_draw_commands_t;

/* ============================================================
 * Theme info (server -> client)
 * ============================================================
 * Sent on connect and on theme hot-reload.
 * Provides 16 semantic colors + sizing/font metrics so
 * clients can build themed UIs without hardcoded values.
 */

/* Semantic color slot indices */
enum {
    VGP_THEME_BG = 0,
    VGP_THEME_BG_SECONDARY,
    VGP_THEME_BG_TERTIARY,
    VGP_THEME_FG,
    VGP_THEME_FG_SECONDARY,
    VGP_THEME_FG_DISABLED,
    VGP_THEME_ACCENT,
    VGP_THEME_ACCENT_HOVER,
    VGP_THEME_BORDER,
    VGP_THEME_ERROR,
    VGP_THEME_SUCCESS,
    VGP_THEME_WARNING,
    VGP_THEME_SCROLLBAR,
    VGP_THEME_SCROLLBAR_THUMB,
    VGP_THEME_SELECTION,
    VGP_THEME_TOOLTIP_BG,
    VGP_THEME_COLOR_COUNT = 16,
};

typedef struct vgp_msg_theme_info {
    vgp_msg_header_t header;  /* type = VGP_MSG_THEME_INFO */
    float colors[16][4];      /* 16 semantic RGBA colors */
    float font_size;          /* default body text size */
    float font_size_small;    /* small/secondary text */
    float font_size_large;    /* headings */
    float corner_radius;      /* standard rounding */
    float padding;            /* standard padding */
    float spacing;            /* standard element spacing */
    float border_width;       /* standard border width */
    float scrollbar_width;    /* scrollbar track width */
    float button_height;      /* standard button height */
    float input_height;       /* text input height */
    float checkbox_size;      /* checkbox box size */
    float slider_height;      /* slider track height */
    float char_advances[95];  /* ASCII 32-126 advance widths at font_size */
} __attribute__((packed)) vgp_msg_theme_info_t;

#endif /* VGP_PROTOCOL_H */
