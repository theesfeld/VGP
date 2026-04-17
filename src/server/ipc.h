/* SPDX-License-Identifier: MIT */
#ifndef VGP_IPC_H
#define VGP_IPC_H

#include "loop.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Forward declaration */
struct vgp_server;

#define VGP_MAX_CLIENTS          32
#define VGP_IPC_INITIAL_BUF_SIZE 65536
#define VGP_IPC_MAX_BUF_SIZE     (8 * 1024 * 1024)  /* 8MB max message */

typedef struct vgp_ipc_client {
    int                fd;
    bool               connected;
    uint32_t           client_id;
    vgp_event_source_t source;
    uint8_t           *recv_buf;
    size_t             recv_len;
    size_t             recv_cap;
} vgp_ipc_client_t;

typedef struct vgp_ipc {
    int                listen_fd;
    vgp_event_source_t listen_source;
    char               socket_path[256];

    vgp_ipc_client_t   clients[VGP_MAX_CLIENTS];
    int                 client_count;
    uint32_t            next_client_id;
    bool                initialized;
} vgp_ipc_t;

int  vgp_ipc_init(vgp_ipc_t *ipc, vgp_event_loop_t *loop);
void vgp_ipc_destroy(vgp_ipc_t *ipc, vgp_event_loop_t *loop);
int  vgp_ipc_accept(vgp_ipc_t *ipc, vgp_event_loop_t *loop);
void vgp_ipc_client_dispatch(vgp_ipc_client_t *client,
                              struct vgp_server *server);
void vgp_ipc_client_disconnect(vgp_ipc_t *ipc, vgp_ipc_client_t *client,
                                vgp_event_loop_t *loop);
int  vgp_ipc_send(vgp_ipc_client_t *client, const void *data, size_t len);
vgp_ipc_client_t *vgp_ipc_find_client(vgp_ipc_t *ipc, int fd);

#endif /* VGP_IPC_H */