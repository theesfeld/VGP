#ifndef VGP_HOTRELOAD_H
#define VGP_HOTRELOAD_H

#include "loop.h"
#include <stdbool.h>

struct vgp_server;

typedef struct vgp_hotreload {
    int                inotify_fd;
    vgp_event_source_t source;
    int                theme_wd;     /* watch descriptor for theme dir */
    int                shader_wd;    /* watch descriptor for shader dir */
    bool               initialized;
} vgp_hotreload_t;

int  vgp_hotreload_init(vgp_hotreload_t *hr, vgp_event_loop_t *loop,
                          const char *theme_dir, const char *shader_dir);
void vgp_hotreload_destroy(vgp_hotreload_t *hr, vgp_event_loop_t *loop);
void vgp_hotreload_dispatch(vgp_hotreload_t *hr, struct vgp_server *server);

#endif /* VGP_HOTRELOAD_H */
