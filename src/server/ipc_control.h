#ifndef VGP_IPC_CONTROL_H
#define VGP_IPC_CONTROL_H

/* IPC control protocol for external scripts (like swaymsg).
 * Listens on a separate Unix socket: $XDG_RUNTIME_DIR/vgp-ctl
 * Commands are newline-delimited text:
 *   "get workspaces" -> JSON array of workspace info
 *   "get windows" -> JSON array of window info
 *   "get config <key>" -> config value
 *   "set config <key> <value>" -> set config value
 *   "exec <command>" -> spawn a process
 *   "focus <window_id>" -> focus a window
 *   "workspace <n>" -> switch workspace
 *   "lock" -> lock screen
 *   "reload" -> reload config
 */

#include "loop.h"
#include <stdbool.h>

struct vgp_server;

typedef struct vgp_ipc_control {
    int                listen_fd;
    vgp_event_source_t listen_source;
    char               socket_path[256];
    bool               initialized;
} vgp_ipc_control_t;

int  vgp_ipc_control_init(vgp_ipc_control_t *ctl, vgp_event_loop_t *loop);
void vgp_ipc_control_destroy(vgp_ipc_control_t *ctl, vgp_event_loop_t *loop);
void vgp_ipc_control_handle(vgp_ipc_control_t *ctl, struct vgp_server *server);

#endif /* VGP_IPC_CONTROL_H */
