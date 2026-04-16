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

#endif /* VGP_PROTOCOL_H */
