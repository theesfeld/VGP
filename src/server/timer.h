#ifndef VGP_TIMER_H
#define VGP_TIMER_H

#include "loop.h"
#include <stdint.h>

typedef void (*vgp_timer_cb_t)(void *data);

typedef struct vgp_timer {
    int                fd;
    vgp_event_source_t source;
    vgp_timer_cb_t     callback;
    void              *data;
} vgp_timer_t;

int  vgp_timer_create(vgp_timer_t *timer, vgp_event_loop_t *loop,
                       vgp_timer_cb_t callback, void *data);
int  vgp_timer_arm_oneshot(vgp_timer_t *timer, uint64_t nsec);
int  vgp_timer_arm_repeating(vgp_timer_t *timer, uint64_t interval_nsec);
int  vgp_timer_disarm(vgp_timer_t *timer);
void vgp_timer_destroy(vgp_timer_t *timer, vgp_event_loop_t *loop);

/* Convenience: milliseconds to nanoseconds */
#define VGP_MS_TO_NS(ms) ((uint64_t)(ms) * 1000000ULL)

#endif /* VGP_TIMER_H */
