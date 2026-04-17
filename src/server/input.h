/* SPDX-License-Identifier: MIT */
#ifndef VGP_INPUT_H
#define VGP_INPUT_H

#include "loop.h"
#include <libinput.h>
#include <stdbool.h>

/* Forward declarations */
struct vgp_server;

struct vgp_seat;

typedef struct vgp_input {
    struct libinput    *li;
    int                 fd;
    vgp_event_source_t  source;
    bool                initialized;
    struct vgp_seat    *seat;  /* for device open/close */
} vgp_input_t;

int  vgp_input_init(vgp_input_t *input, vgp_event_loop_t *loop,
                     int tty_fd);
void vgp_input_destroy(vgp_input_t *input, vgp_event_loop_t *loop);
void vgp_input_dispatch(vgp_input_t *input, struct vgp_server *server);

#endif /* VGP_INPUT_H */