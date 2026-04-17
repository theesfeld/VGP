/* SPDX-License-Identifier: MIT */
#ifndef VGP_LOOP_H
#define VGP_LOOP_H

#include <stdint.h>
#include <stdbool.h>

/* Forward declaration */
struct vgp_server;

typedef enum {
    VGP_EVENT_DRM,
    VGP_EVENT_INPUT,
    VGP_EVENT_IPC_LISTEN,
    VGP_EVENT_IPC_CLIENT,
    VGP_EVENT_TIMER,
    VGP_EVENT_SIGNAL,
} vgp_event_type_t;

typedef struct vgp_event_source {
    vgp_event_type_t type;
    int              fd;
    void            *data;
} vgp_event_source_t;

typedef struct vgp_event_loop {
    int  epoll_fd;
    bool running;
} vgp_event_loop_t;

int  vgp_event_loop_init(vgp_event_loop_t *loop);
void vgp_event_loop_destroy(vgp_event_loop_t *loop);

int  vgp_event_loop_add_fd(vgp_event_loop_t *loop, int fd,
                            uint32_t events, vgp_event_source_t *source);
int  vgp_event_loop_mod_fd(vgp_event_loop_t *loop, int fd,
                            uint32_t events, vgp_event_source_t *source);
int  vgp_event_loop_del_fd(vgp_event_loop_t *loop, int fd);

void vgp_event_loop_run(vgp_event_loop_t *loop, struct vgp_server *server);
void vgp_event_loop_stop(vgp_event_loop_t *loop);

#endif /* VGP_LOOP_H */