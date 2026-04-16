#include "vgp/vgp.h"
#include "vgp/protocol.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <stdio.h>

#define RECV_BUF_SIZE 65536

struct vgp_connection {
    int                  fd;
    vgp_display_info_t   display;
    vgp_event_callback_t event_cb;
    void                *event_cb_data;
    uint8_t              recv_buf[RECV_BUF_SIZE];
    size_t               recv_len;
    bool                 connected;
};

/* ============================================================
 * Internal helpers
 * ============================================================ */

static int send_all(int fd, const void *data, size_t len)
{
    const uint8_t *ptr = data;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, ptr + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

static ssize_t recv_msg(vgp_connection_t *conn, void *buf, size_t max_len,
                         int timeout_ms)
{
    struct pollfd pfd = { .fd = conn->fd, .events = POLLIN };
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret <= 0)
        return ret;

    return read(conn->fd, buf, max_len);
}

/* ============================================================
 * Connection
 * ============================================================ */

vgp_connection_t *vgp_connect(const char *socket_path)
{
    vgp_connection_t *conn = calloc(1, sizeof(*conn));
    if (!conn) return NULL;
    conn->fd = -1;

    /* Determine socket path */
    char path_buf[256];
    if (!socket_path) {
        const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
        if (!runtime_dir) runtime_dir = "/tmp";
        snprintf(path_buf, sizeof(path_buf), "%s/vgp-0", runtime_dir);
        socket_path = path_buf;
    }

    /* Create socket */
    conn->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (conn->fd < 0) {
        free(conn);
        return NULL;
    }

    /* Connect */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(conn->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    /* Send handshake */
    vgp_msg_connect_t msg = {
        .header = {
            .magic = VGP_PROTOCOL_MAGIC,
            .type = VGP_MSG_CONNECT,
            .length = sizeof(msg),
            .window_id = 0,
        },
        .protocol_version = VGP_PROTOCOL_VERSION,
        .client_pid = (uint32_t)getpid(),
    };
    strncpy(msg.app_id, "vgp-client", sizeof(msg.app_id) - 1);

    if (send_all(conn->fd, &msg, sizeof(msg)) < 0) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    /* Wait for connect OK */
    vgp_msg_connect_ok_t reply;
    ssize_t n = recv_msg(conn, &reply, sizeof(reply), 5000);
    if (n < (ssize_t)sizeof(reply) || reply.header.type != VGP_MSG_CONNECT_OK) {
        close(conn->fd);
        free(conn);
        return NULL;
    }

    conn->display.width = reply.display_width;
    conn->display.height = reply.display_height;
    conn->display.client_id = reply.client_id;
    conn->connected = true;

    /* Set non-blocking for event processing */
    int flags = fcntl(conn->fd, F_GETFL);
    fcntl(conn->fd, F_SETFL, flags | O_NONBLOCK);

    return conn;
}

void vgp_disconnect(vgp_connection_t *conn)
{
    if (!conn) return;

    if (conn->fd >= 0) {
        /* Send disconnect message */
        vgp_msg_header_t msg = {
            .magic = VGP_PROTOCOL_MAGIC,
            .type = VGP_MSG_DISCONNECT,
            .length = sizeof(msg),
        };
        send_all(conn->fd, &msg, sizeof(msg));
        close(conn->fd);
    }

    free(conn);
}

int vgp_fd(vgp_connection_t *conn)
{
    return conn ? conn->fd : -1;
}

vgp_display_info_t vgp_get_display_info(vgp_connection_t *conn)
{
    return conn->display;
}

void vgp_set_event_callback(vgp_connection_t *conn,
                             vgp_event_callback_t callback,
                             void *user_data)
{
    conn->event_cb = callback;
    conn->event_cb_data = user_data;
}

/* Process received messages */
static void process_messages(vgp_connection_t *conn)
{
    while (conn->recv_len >= sizeof(vgp_msg_header_t)) {
        vgp_msg_header_t *hdr = (vgp_msg_header_t *)conn->recv_buf;

        if (hdr->magic != VGP_PROTOCOL_MAGIC) {
            conn->recv_len = 0;
            return;
        }

        if (conn->recv_len < hdr->length)
            break;

        /* Dispatch event to callback */
        if (conn->event_cb) {
            vgp_event_t ev = { .window_id = hdr->window_id };

            switch (hdr->type) {
            case VGP_MSG_KEY_PRESS:
            case VGP_MSG_KEY_RELEASE: {
                vgp_msg_key_event_t *key = (vgp_msg_key_event_t *)hdr;
                ev.type = hdr->type == VGP_MSG_KEY_PRESS ?
                    VGP_EVENT_KEY_PRESS : VGP_EVENT_KEY_RELEASE;
                ev.key.keycode = key->keycode;
                ev.key.keysym = key->keysym;
                ev.key.modifiers = key->modifiers;
                ev.key.utf8_len = key->utf8_len;
                memcpy(ev.key.utf8, key->utf8, sizeof(ev.key.utf8));
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            }
            case VGP_MSG_MOUSE_MOVE: {
                vgp_msg_mouse_move_event_t *mm = (vgp_msg_mouse_move_event_t *)hdr;
                ev.type = VGP_EVENT_MOUSE_MOVE;
                ev.mouse_move.x = mm->x;
                ev.mouse_move.y = mm->y;
                ev.mouse_move.modifiers = mm->modifiers;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            }
            case VGP_MSG_MOUSE_BUTTON: {
                vgp_msg_mouse_button_event_t *mb = (vgp_msg_mouse_button_event_t *)hdr;
                ev.type = VGP_EVENT_MOUSE_BUTTON;
                ev.mouse_button.x = mb->x;
                ev.mouse_button.y = mb->y;
                ev.mouse_button.button = mb->button;
                ev.mouse_button.pressed = mb->state != 0;
                ev.mouse_button.modifiers = mb->modifiers;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            }
            case VGP_MSG_MOUSE_SCROLL: {
                vgp_msg_mouse_scroll_event_t *ms = (vgp_msg_mouse_scroll_event_t *)hdr;
                ev.type = VGP_EVENT_MOUSE_SCROLL;
                ev.scroll.dx = ms->dx;
                ev.scroll.dy = ms->dy;
                ev.scroll.modifiers = ms->modifiers;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            }
            case VGP_MSG_FOCUS_IN:
                ev.type = VGP_EVENT_FOCUS_IN;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            case VGP_MSG_FOCUS_OUT:
                ev.type = VGP_EVENT_FOCUS_OUT;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            case VGP_MSG_WINDOW_CONFIGURE: {
                vgp_msg_window_configure_t *cfg = (vgp_msg_window_configure_t *)hdr;
                ev.type = VGP_EVENT_CONFIGURE;
                ev.configure.x = cfg->x;
                ev.configure.y = cfg->y;
                ev.configure.width = cfg->width;
                ev.configure.height = cfg->height;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            }
            case VGP_MSG_WINDOW_CLOSE:
                ev.type = VGP_EVENT_CLOSE;
                conn->event_cb(conn, &ev, conn->event_cb_data);
                break;
            default:
                break;
            }
        }

        /* Consume message */
        size_t remaining = conn->recv_len - hdr->length;
        if (remaining > 0)
            memmove(conn->recv_buf, conn->recv_buf + hdr->length, remaining);
        conn->recv_len = remaining;
    }
}

int vgp_dispatch(vgp_connection_t *conn)
{
    if (!conn || !conn->connected) {
        fprintf(stderr, "  dispatch: not connected\n");
        return -1;
    }

    size_t space = RECV_BUF_SIZE - conn->recv_len;
    if (space == 0) return 0;

    ssize_t n = read(conn->fd, conn->recv_buf + conn->recv_len, space);
    if (n > 0) {
        conn->recv_len += (size_t)n;
        process_messages(conn);
        return (int)n;
    } else if (n == 0) {
        fprintf(stderr, "  dispatch: read EOF (server closed connection)\n");
        conn->connected = false;
        return -1;
    } else {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        fprintf(stderr, "  dispatch: read error: %s\n", strerror(errno));
        conn->connected = false;
        return -1;
    }
}

void vgp_run(vgp_connection_t *conn)
{
    if (!conn) return;

    while (conn->connected) {
        struct pollfd pfd = { .fd = conn->fd, .events = POLLIN };
        int ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret > 0)
            vgp_dispatch(conn);
    }
}

/* ============================================================
 * Windows
 * ============================================================ */

uint32_t vgp_window_create(vgp_connection_t *conn,
                             int32_t x, int32_t y,
                             uint32_t width, uint32_t height,
                             const char *title, uint32_t flags)
{
    if (!conn || !conn->connected) return 0;

    size_t title_len = title ? strlen(title) : 0;
    size_t msg_size = sizeof(vgp_msg_window_create_t) + title_len;

    uint8_t buf[512];
    if (msg_size > sizeof(buf)) return 0;

    vgp_msg_window_create_t *msg = (vgp_msg_window_create_t *)buf;
    *msg = (vgp_msg_window_create_t){
        .header = {
            .magic = VGP_PROTOCOL_MAGIC,
            .type = VGP_MSG_WINDOW_CREATE,
            .length = (uint32_t)msg_size,
            .window_id = 0,
        },
        .x = x, .y = y,
        .width = width, .height = height,
        .flags = flags,
        .title_len = (uint32_t)title_len,
    };
    if (title_len > 0)
        memcpy(buf + sizeof(vgp_msg_window_create_t), title, title_len);

    if (send_all(conn->fd, buf, msg_size) < 0)
        return 0;

    /* Wait for WINDOW_CREATED response */
    /* Temporarily switch to blocking for this read */
    int flags_fd = fcntl(conn->fd, F_GETFL);
    fcntl(conn->fd, F_SETFL, flags_fd & ~O_NONBLOCK);

    vgp_msg_window_created_t reply;
    ssize_t n = recv_msg(conn, &reply, sizeof(reply), 5000);

    fcntl(conn->fd, F_SETFL, flags_fd); /* restore */

    if (n < (ssize_t)sizeof(reply) || reply.header.type != VGP_MSG_WINDOW_CREATED)
        return 0;

    return reply.window_id;
}

void vgp_window_destroy(vgp_connection_t *conn, uint32_t window_id)
{
    if (!conn || !conn->connected) return;

    vgp_msg_header_t msg = {
        .magic = VGP_PROTOCOL_MAGIC,
        .type = VGP_MSG_WINDOW_DESTROY,
        .length = sizeof(msg),
        .window_id = window_id,
    };
    send_all(conn->fd, &msg, sizeof(msg));
}

void vgp_window_set_title(vgp_connection_t *conn, uint32_t window_id,
                           const char *title)
{
    if (!conn || !conn->connected || !title) return;

    size_t title_len = strlen(title);
    size_t msg_size = sizeof(vgp_msg_header_t) + title_len;
    uint8_t buf[512];
    if (msg_size > sizeof(buf)) return;

    vgp_msg_header_t *hdr = (vgp_msg_header_t *)buf;
    *hdr = (vgp_msg_header_t){
        .magic = VGP_PROTOCOL_MAGIC,
        .type = VGP_MSG_WINDOW_SET_TITLE,
        .length = (uint32_t)msg_size,
        .window_id = window_id,
    };
    memcpy(buf + sizeof(vgp_msg_header_t), title, title_len);

    send_all(conn->fd, buf, msg_size);
}

/* ============================================================
 * Surface
 * ============================================================ */

void vgp_surface_attach(vgp_connection_t *conn, uint32_t window_id,
                         uint32_t width, uint32_t height,
                         uint32_t stride, const uint8_t *data)
{
    if (!conn || !conn->connected || !data) return;

    size_t pixel_size = (size_t)stride * height;
    size_t msg_size = sizeof(vgp_msg_surface_attach_t) + pixel_size;

    /* Allocate buffer for the message */
    uint8_t *buf = malloc(msg_size);
    if (!buf) return;

    vgp_msg_surface_attach_t *msg = (vgp_msg_surface_attach_t *)buf;
    *msg = (vgp_msg_surface_attach_t){
        .header = {
            .magic = VGP_PROTOCOL_MAGIC,
            .type = VGP_MSG_SURFACE_ATTACH,
            .length = (uint32_t)msg_size,
            .window_id = window_id,
        },
        .width = width,
        .height = height,
        .stride = stride,
        .format = 0, /* ARGB8888 */
    };
    memcpy(buf + sizeof(vgp_msg_surface_attach_t), data, pixel_size);

    send_all(conn->fd, buf, msg_size);
    free(buf);
}

/* ============================================================
 * Cell Grid (vector terminal)
 * ============================================================ */

void vgp_cellgrid_send(vgp_connection_t *conn, uint32_t window_id,
                        uint16_t rows, uint16_t cols,
                        uint16_t cursor_row, uint16_t cursor_col,
                        uint8_t cursor_visible, uint8_t cursor_shape,
                        const void *cells)
{
    if (!conn || !conn->connected || !cells) return;

    size_t cell_data_size = (size_t)rows * cols * 12; /* sizeof(vgp_cell_t) = 12 */
    size_t msg_size = sizeof(vgp_msg_cellgrid_t) + cell_data_size;

    uint8_t *buf = malloc(msg_size);
    if (!buf) return;

    vgp_msg_cellgrid_t *msg = (vgp_msg_cellgrid_t *)buf;
    msg->header = (vgp_msg_header_t){
        .magic = VGP_PROTOCOL_MAGIC,
        .type = 0x0090, /* VGP_MSG_CELLGRID */
        .length = (uint32_t)msg_size,
        .window_id = window_id,
    };
    msg->rows = rows;
    msg->cols = cols;
    msg->cursor_row = cursor_row;
    msg->cursor_col = cursor_col;
    msg->cursor_visible = cursor_visible;
    msg->cursor_shape = cursor_shape;
    msg->_pad[0] = msg->_pad[1] = 0;

    memcpy(buf + sizeof(vgp_msg_cellgrid_t), cells, cell_data_size);
    send_all(conn->fd, buf, msg_size);
    free(buf);
}

/* ============================================================
 * Clipboard
 * ============================================================ */

void vgp_clipboard_set(vgp_connection_t *conn, const char *text, size_t len)
{
    if (!conn || !conn->connected || !text) return;
    size_t msg_size = sizeof(vgp_msg_header_t) + len;
    uint8_t *buf = malloc(msg_size);
    if (!buf) return;
    vgp_msg_header_t *hdr = (vgp_msg_header_t *)buf;
    *hdr = (vgp_msg_header_t){
        .magic = VGP_PROTOCOL_MAGIC,
        .type = VGP_MSG_CLIPBOARD_SET,
        .length = (uint32_t)msg_size,
    };
    memcpy(buf + sizeof(vgp_msg_header_t), text, len);
    send_all(conn->fd, buf, msg_size);
    free(buf);
}

char *vgp_clipboard_get(vgp_connection_t *conn, size_t *out_len)
{
    if (!conn || !conn->connected) return NULL;

    /* Send request */
    vgp_msg_header_t req = {
        .magic = VGP_PROTOCOL_MAGIC,
        .type = VGP_MSG_CLIPBOARD_GET,
        .length = sizeof(req),
    };
    send_all(conn->fd, &req, sizeof(req));

    /* Wait for response (brief blocking read) */
    int flags = fcntl(conn->fd, F_GETFL);
    fcntl(conn->fd, F_SETFL, flags & ~O_NONBLOCK);

    vgp_msg_header_t reply_hdr;
    ssize_t n = recv_msg(conn, &reply_hdr, sizeof(reply_hdr), 1000);
    if (n < (ssize_t)sizeof(reply_hdr) || reply_hdr.type != VGP_MSG_CLIPBOARD_DATA) {
        fcntl(conn->fd, F_SETFL, flags);
        return NULL;
    }

    size_t data_len = reply_hdr.length - sizeof(vgp_msg_header_t);
    char *data = NULL;
    if (data_len > 0) {
        data = malloc(data_len + 1);
        if (data) {
            ssize_t r = recv_msg(conn, data, data_len, 1000);
            if (r < (ssize_t)data_len) { free(data); data = NULL; }
            else { data[data_len] = '\0'; if (out_len) *out_len = data_len; }
        }
    }

    fcntl(conn->fd, F_SETFL, flags);
    return data;
}

void vgp_open_url(vgp_connection_t *conn, const char *url)
{
    if (!conn || !conn->connected || !url) return;
    size_t url_len = strlen(url);
    size_t msg_size = sizeof(vgp_msg_header_t) + url_len;
    uint8_t *buf = malloc(msg_size);
    if (!buf) return;
    vgp_msg_header_t *hdr = (vgp_msg_header_t *)buf;
    *hdr = (vgp_msg_header_t){
        .magic = VGP_PROTOCOL_MAGIC,
        .type = VGP_MSG_OPEN_URL,
        .length = (uint32_t)msg_size,
    };
    memcpy(buf + sizeof(vgp_msg_header_t), url, url_len);
    send_all(conn->fd, buf, msg_size);
    free(buf);
}
