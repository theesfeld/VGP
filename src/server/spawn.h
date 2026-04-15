#ifndef VGP_SPAWN_H
#define VGP_SPAWN_H

struct vgp_server;

/* Spawn a child process via /bin/sh -c. Returns child PID or -1. */
int vgp_spawn(struct vgp_server *server, const char *cmd);

/* Reap zombie children (call from SIGCHLD handler). */
void vgp_spawn_reap_children(void);

#endif /* VGP_SPAWN_H */
