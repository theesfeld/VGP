/* SPDX-License-Identifier: MIT */
#ifndef VGP_LOCKSCREEN_H
#define VGP_LOCKSCREEN_H

#include "render_backend.h"
#include "timer.h"
#include "loop.h"
#include <stdbool.h>

struct vgp_server;

typedef struct vgp_lockscreen {
    bool     enabled;        /* from config */
    int      timeout_min;    /* minutes of idle before lock */
    bool     locked;         /* currently locked */
    bool     show_input;     /* password input visible */
    char     password[256];  /* typed password */
    int      password_len;
    float    idle_seconds;   /* time since last input */
    char     status[64];     /* "Wrong password" etc. */
    int      status_timer;
    int      failed_attempts;
} vgp_lockscreen_t;

void vgp_lockscreen_init(vgp_lockscreen_t *ls, bool enabled, int timeout_min);
void vgp_lockscreen_tick(vgp_lockscreen_t *ls, float dt);
void vgp_lockscreen_input_activity(vgp_lockscreen_t *ls);
void vgp_lockscreen_lock(vgp_lockscreen_t *ls);
void vgp_lockscreen_key(vgp_lockscreen_t *ls, uint32_t keysym,
                         const char *utf8, int utf8_len);

/* Returns true if the lock screen is active (blocks all other input) */
bool vgp_lockscreen_is_locked(vgp_lockscreen_t *ls);

/* Render the lock screen overlay */
void vgp_lockscreen_render(vgp_lockscreen_t *ls, void *backend, void *ctx,
                            float width, float height, float time);

#endif /* VGP_LOCKSCREEN_H */