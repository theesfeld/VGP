#ifndef VGP_H
#define VGP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Version
 * ============================================================ */

#define VGP_VERSION_MAJOR 0
#define VGP_VERSION_MINOR 1
#define VGP_VERSION_PATCH 0

/* ============================================================
 * Opaque types
 * ============================================================ */

typedef struct vgp_connection vgp_connection_t;
typedef struct vgp_window     vgp_window_t;

/* ============================================================
 * Value types
 * ============================================================ */

typedef struct vgp_display_info {
    uint32_t width;
    uint32_t height;
    uint32_t client_id;
} vgp_display_info_t;

/* ============================================================
 * Event types
 * ============================================================ */

typedef enum {
    VGP_EVENT_KEY_PRESS,
    VGP_EVENT_KEY_RELEASE,
    VGP_EVENT_MOUSE_MOVE,
    VGP_EVENT_MOUSE_BUTTON,
    VGP_EVENT_MOUSE_SCROLL,
    VGP_EVENT_FOCUS_IN,
    VGP_EVENT_FOCUS_OUT,
    VGP_EVENT_CONFIGURE,
    VGP_EVENT_CLOSE,
} vgp_event_type_t;

typedef struct vgp_event {
    vgp_event_type_t type;
    uint32_t         window_id;

    union {
        struct {
            uint32_t keycode;
            uint32_t keysym;
            uint32_t modifiers;
            char     utf8[8];
            uint32_t utf8_len;
        } key;

        struct {
            float    x, y;
            uint32_t button;
            bool     pressed;
            uint32_t modifiers;
        } mouse_button;

        struct {
            float    x, y;
            uint32_t modifiers;
        } mouse_move;

        struct {
            float    dx, dy;
            uint32_t modifiers;
        } scroll;

        struct {
            uint32_t width, height;
            int32_t  x, y;
        } configure;
    };
} vgp_event_t;

/* Event callback */
typedef void (*vgp_event_callback_t)(vgp_connection_t *conn,
                                      const vgp_event_t *event,
                                      void *user_data);

/* ============================================================
 * Connection API
 * ============================================================ */

/* Connect to VGP server. Returns NULL on failure.
 * If socket_path is NULL, uses $XDG_RUNTIME_DIR/vgp-0 */
vgp_connection_t *vgp_connect(const char *socket_path);

/* Disconnect from server. */
void vgp_disconnect(vgp_connection_t *conn);

/* Get the socket fd for use in external event loops. */
int vgp_fd(vgp_connection_t *conn);

/* Get display info (screen size, etc.) */
vgp_display_info_t vgp_get_display_info(vgp_connection_t *conn);

/* Set event callback */
void vgp_set_event_callback(vgp_connection_t *conn,
                             vgp_event_callback_t callback,
                             void *user_data);

/* Process pending events (non-blocking). Returns number of events processed. */
int vgp_dispatch(vgp_connection_t *conn);

/* Run the event loop (blocking). Returns when connection closes. */
void vgp_run(vgp_connection_t *conn);

/* ============================================================
 * Window API
 * ============================================================ */

#define VGP_WINDOW_DECORATED  0x0001
#define VGP_WINDOW_RESIZABLE  0x0002
#define VGP_WINDOW_OVERRIDE   0x0004  /* no decorations, always on top */

/* Create a window. Returns window ID (0 on failure). */
uint32_t vgp_window_create(vgp_connection_t *conn,
                             int32_t x, int32_t y,
                             uint32_t width, uint32_t height,
                             const char *title, uint32_t flags);

/* Destroy a window. */
void vgp_window_destroy(vgp_connection_t *conn, uint32_t window_id);

/* Set window title. */
void vgp_window_set_title(vgp_connection_t *conn, uint32_t window_id,
                           const char *title);

/* ============================================================
 * Surface API (client-side rendering)
 * ============================================================ */

/* Send pixel data for a window. Format is ARGB8888 (premultiplied).
 * data must contain stride * height bytes. */
void vgp_surface_attach(vgp_connection_t *conn, uint32_t window_id,
                         uint32_t width, uint32_t height,
                         uint32_t stride, const uint8_t *data);

/* ============================================================
 * Terminal Cell Grid API (vector text protocol)
 * ============================================================ */

/* Send terminal cell grid for server-side vector rendering.
 * cells array must be rows * cols elements. */
void vgp_cellgrid_send(vgp_connection_t *conn, uint32_t window_id,
                        uint16_t rows, uint16_t cols,
                        uint16_t cursor_row, uint16_t cursor_col,
                        uint8_t cursor_visible, uint8_t cursor_shape,
                        const void *cells);

/* ============================================================
 * Clipboard API
 * ============================================================ */

/* Set clipboard content (UTF-8 text). */
void vgp_clipboard_set(vgp_connection_t *conn, const char *text, size_t len);

/* Request clipboard content. Returns malloc'd string or NULL.
 * Caller must free(). Blocks briefly for server response. */
char *vgp_clipboard_get(vgp_connection_t *conn, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* VGP_H */
