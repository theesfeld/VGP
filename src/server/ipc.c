/* SPDX-License-Identifier: MIT */
#include "ipc.h"
#include "server.h"
#include "vgp/log.h"
#include "vgp/protocol.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define TAG "ipc"

int vgp_ipc_init(vgp_ipc_t *ipc, vgp_event_loop_t *loop)
{
    memset(ipc, 0, sizeof(*ipc));
    ipc->listen_fd = -1;
    ipc->next_client_id = 1;

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir)
        runtime_dir = "/tmp";
    snprintf(ipc->socket_path, sizeof(ipc->socket_path), "%s/vgp-0", runtime_dir);

    unlink(ipc->socket_path);

    ipc->listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (ipc->listen_fd < 0) {
        VGP_LOG_ERRNO(TAG, "socket failed");
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ipc->socket_path, sizeof(addr.sun_path) - 1);

    if (bind(ipc->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        VGP_LOG_ERRNO(TAG, "bind %s failed", ipc->socket_path);
        close(ipc->listen_fd);
        ipc->listen_fd = -1;
        return -1;
    }

    if (listen(ipc->listen_fd, 8) < 0) {
        VGP_LOG_ERRNO(TAG, "listen failed");
        close(ipc->listen_fd);
        ipc->listen_fd = -1;
        return -1;
    }

    ipc->listen_source.type = VGP_EVENT_IPC_LISTEN;
    ipc->listen_source.fd = ipc->listen_fd;
    ipc->listen_source.data = ipc;

    if (vgp_event_loop_add_fd(loop, ipc->listen_fd, EPOLLIN, &ipc->listen_source) < 0) {
        close(ipc->listen_fd);
        ipc->listen_fd = -1;
        return -1;
    }

    ipc->initialized = true;
    VGP_LOG_INFO(TAG, "listening on %s", ipc->socket_path);
    return 0;
}

void vgp_ipc_destroy(vgp_ipc_t *ipc, vgp_event_loop_t *loop)
{
    if (!ipc->initialized)
        return;

    for (int i = 0; i < VGP_MAX_CLIENTS; i++) {
        if (ipc->clients[i].connected) {
            vgp_event_loop_del_fd(loop, ipc->clients[i].fd);
            close(ipc->clients[i].fd);
            free(ipc->clients[i].recv_buf);
            ipc->clients[i].recv_buf = NULL;
        }
    }

    if (ipc->listen_fd >= 0) {
        vgp_event_loop_del_fd(loop, ipc->listen_fd);
        close(ipc->listen_fd);
        unlink(ipc->socket_path);
    }

    ipc->initialized = false;
}

int vgp_ipc_accept(vgp_ipc_t *ipc, vgp_event_loop_t *loop)
{
    int client_fd = accept4(ipc->listen_fd, NULL, NULL,
                             SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            VGP_LOG_ERRNO(TAG, "accept failed");
        return -1;
    }

    vgp_ipc_client_t *client = NULL;
    for (int i = 0; i < VGP_MAX_CLIENTS; i++) {
        if (!ipc->clients[i].connected) {
            client = &ipc->clients[i];
            break;
        }
    }

    if (!client) {
        VGP_LOG_WARN(TAG, "max clients reached, rejecting");
        close(client_fd);
        return -1;
    }

    memset(client, 0, sizeof(*client));
    client->fd = client_fd;
    client->connected = true;
    client->client_id = ipc->next_client_id++;
    client->source.type = VGP_EVENT_IPC_CLIENT;
    client->source.fd = client_fd;
    client->source.data = client;

    /* Allocate initial recv buffer */
    client->recv_buf = malloc(VGP_IPC_INITIAL_BUF_SIZE);
    if (!client->recv_buf) {
        close(client_fd);
        client->connected = false;
        return -1;
    }
    client->recv_cap = VGP_IPC_INITIAL_BUF_SIZE;
    client->recv_len = 0;

    if (vgp_event_loop_add_fd(loop, client_fd, EPOLLIN, &client->source) < 0) {
        free(client->recv_buf);
        close(client_fd);
        client->connected = false;
        return -1;
    }

    ipc->client_count++;
    VGP_LOG_INFO(TAG, "client %u connected (fd=%d)", client->client_id, client_fd);
    return 0;
}

/* Grow the recv buffer if needed to fit a message of the given size */
static int ensure_recv_capacity(vgp_ipc_client_t *client, size_t needed)
{
    if (needed <= client->recv_cap)
        return 0;

    if (needed > VGP_IPC_MAX_BUF_SIZE) {
        VGP_LOG_WARN(TAG, "client %u message too large (%zu bytes)",
                     client->client_id, needed);
        return -1;
    }

    /* Double until big enough */
    size_t new_cap = client->recv_cap;
    while (new_cap < needed)
        new_cap *= 2;
    if (new_cap > VGP_IPC_MAX_BUF_SIZE)
        new_cap = VGP_IPC_MAX_BUF_SIZE;

    uint8_t *new_buf = realloc(client->recv_buf, new_cap);
    if (!new_buf) return -1;

    client->recv_buf = new_buf;
    client->recv_cap = new_cap;
    return 0;
}

void vgp_ipc_client_dispatch(vgp_ipc_client_t *client,
                              struct vgp_server *server)
{
    /* Read ALL available data in a loop (don't rely on one read per epoll) */
    for (;;) {
        size_t space = client->recv_cap - client->recv_len;
        if (space < 4096) {
            if (ensure_recv_capacity(client, client->recv_cap * 2) < 0) {
                VGP_LOG_WARN(TAG, "client %u buffer at max", client->client_id);
                break;
            }
            space = client->recv_cap - client->recv_len;
        }

        ssize_t n = read(client->fd, client->recv_buf + client->recv_len, space);
        if (n <= 0) {
            if (n == 0) {
                VGP_LOG_INFO(TAG, "client %u disconnected (EOF)", client->client_id);
                vgp_server_handle_client_disconnect(server, client);
                return;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; /* no more data right now */
            VGP_LOG_INFO(TAG, "client %u disconnected (error)", client->client_id);
            vgp_server_handle_client_disconnect(server, client);
            return;
        }

        client->recv_len += (size_t)n;
    }

    /* Process all complete messages */
    while (client->recv_len >= sizeof(vgp_msg_header_t)) {
        vgp_msg_header_t *hdr = (vgp_msg_header_t *)client->recv_buf;

        if (hdr->magic != VGP_PROTOCOL_MAGIC) {
            VGP_LOG_WARN(TAG, "client %u bad magic 0x%08x",
                         client->client_id, hdr->magic);
            client->recv_len = 0;
            return;
        }

        /* Reject messages with invalid length */
        if (hdr->length < sizeof(vgp_msg_header_t) ||
            hdr->length > VGP_IPC_MAX_BUF_SIZE) {
            VGP_LOG_WARN(TAG, "client %u invalid msg length %u",
                         client->client_id, hdr->length);
            client->recv_len = 0;
            return;
        }

        /* Grow buffer if needed for large messages (e.g. surface_attach) */
        if (hdr->length > client->recv_cap) {
            if (ensure_recv_capacity(client, hdr->length) < 0) {
                VGP_LOG_WARN(TAG, "client %u message too large (%u), dropping",
                             client->client_id, hdr->length);
                client->recv_len = 0;
                return;
            }
            hdr = (vgp_msg_header_t *)client->recv_buf;
        }

        /* Wait for the full message to arrive */
        if (client->recv_len < hdr->length)
            break;

        vgp_server_handle_message(server, client, hdr);

        size_t remaining = client->recv_len - hdr->length;
        if (remaining > 0)
            memmove(client->recv_buf, client->recv_buf + hdr->length, remaining);
        client->recv_len = remaining;
    }
}

void vgp_ipc_client_disconnect(vgp_ipc_t *ipc, vgp_ipc_client_t *client,
                                vgp_event_loop_t *loop)
{
    if (!client->connected)
        return;

    vgp_event_loop_del_fd(loop, client->fd);
    close(client->fd);
    free(client->recv_buf);
    client->recv_buf = NULL;
    client->recv_cap = 0;
    client->connected = false;
    client->fd = -1;
    client->recv_len = 0;
    ipc->client_count--;
}

int vgp_ipc_send(vgp_ipc_client_t *client, const void *data, size_t len)
{
    if (!client->connected)
        return -1;

    const uint8_t *ptr = data;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(client->fd, ptr + sent, len - sent);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            return -1;
        }
        sent += (size_t)n;
    }
    return 0;
}

vgp_ipc_client_t *vgp_ipc_find_client(vgp_ipc_t *ipc, int fd)
{
    for (int i = 0; i < VGP_MAX_CLIENTS; i++) {
        if (ipc->clients[i].connected && ipc->clients[i].fd == fd)
            return &ipc->clients[i];
    }
    return NULL;
}