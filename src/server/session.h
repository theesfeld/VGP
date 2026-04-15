#ifndef VGP_SESSION_H
#define VGP_SESSION_H

struct vgp_server;

/* Save current window layout to ~/.config/vgp/session.json */
int vgp_session_save(struct vgp_server *server);

/* Restore window layout (spawns saved programs) */
int vgp_session_restore(struct vgp_server *server);

#endif /* VGP_SESSION_H */
