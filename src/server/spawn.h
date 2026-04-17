#ifndef VGP_SPAWN_H
#define VGP_SPAWN_H

struct vgp_server;

/* Spawn a child process via /bin/sh -c. Returns child PID or -1. */
int vgp_spawn(struct vgp_server *server, const char *cmd);

/* Reap zombie children (call from SIGCHLD handler). */
void vgp_spawn_reap_children(void);

/* XDG Autostart: scan $XDG_CONFIG_HOME/autostart and each directory
 * in $XDG_CONFIG_DIRS for .desktop files. Each entry is parsed for
 * Exec= and Hidden=; Hidden=true entries are skipped, and entries
 * whose filename was already seen in a higher-priority directory are
 * also skipped (per the XDG Autostart Spec). */
void vgp_autostart_xdg(struct vgp_server *server);

#endif /* VGP_SPAWN_H */
