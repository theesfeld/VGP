#ifndef VGP_SEAT_H
#define VGP_SEAT_H

#include "loop.h"
#include <stdbool.h>

struct vgp_server;

typedef struct vgp_seat {
    struct libseat     *seat;
    int                 seat_fd;       /* libseat event fd for epoll */
    vgp_event_source_t  seat_source;
    bool                active;        /* true when we have the seat */
    bool                initialized;
    struct vgp_server  *server;        /* back-pointer for callbacks */
} vgp_seat_t;

int  vgp_seat_init(vgp_seat_t *s, vgp_event_loop_t *loop,
                    struct vgp_server *server);
void vgp_seat_destroy(vgp_seat_t *s, vgp_event_loop_t *loop);
void vgp_seat_dispatch(vgp_seat_t *s);

/* Open a device through logind (DRM, input devices).
 * Returns fd on success, -1 on failure. device_id is stored for close. */
int  vgp_seat_open_device(vgp_seat_t *s, const char *path, int *device_id);
void vgp_seat_close_device(vgp_seat_t *s, int device_id);

#endif /* VGP_SEAT_H */
