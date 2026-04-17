/* SPDX-License-Identifier: MIT */
#ifndef VGP_SESSION_H
#define VGP_SESSION_H

#include "vgp/types.h"
#include <stdbool.h>

#define VGP_SESSION_MAX_ENTRIES 64

struct vgp_server;

typedef struct vgp_session_entry {
    char       title[256];
    int        workspace;
    vgp_rect_t rect;
    bool       floating;
    bool       matched;   /* true if a window has claimed this slot */
} vgp_session_entry_t;

typedef struct vgp_session {
    vgp_session_entry_t entries[VGP_SESSION_MAX_ENTRIES];
    int                 count;
    bool                restoring; /* true if we loaded a session and are matching windows */
} vgp_session_t;

/* Save current window layout to $XDG_STATE_HOME/vgp/session.json */
int vgp_session_save(struct vgp_server *server);

/* Load saved session layout (does not spawn; call match on new windows) */
int vgp_session_load(vgp_session_t *session);

/* Try to match a newly created window to a saved session entry.
 * Returns true if matched (window was repositioned). */
bool vgp_session_match_window(vgp_session_t *session, const char *title,
                               vgp_rect_t *rect_out, int *workspace_out,
                               bool *floating_out);

#endif /* VGP_SESSION_H */