#ifndef VGP_VT_H
#define VGP_VT_H

#include "loop.h"
#include <stdbool.h>

/* Forward declaration */
struct vgp_server;

typedef struct vgp_vt {
    int  tty_fd;
    int  vt_num;
    int  saved_kb_mode;
    bool active;
    int  signal_fd;
    vgp_event_source_t signal_source;
} vgp_vt_t;

int  vgp_vt_init(vgp_vt_t *vt, vgp_event_loop_t *loop);
void vgp_vt_destroy(vgp_vt_t *vt, vgp_event_loop_t *loop);
void vgp_vt_handle_signal(vgp_vt_t *vt, struct vgp_server *server);

#endif /* VGP_VT_H */
