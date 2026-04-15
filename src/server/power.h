#ifndef VGP_POWER_H
#define VGP_POWER_H

#include <stdbool.h>

struct vgp_server;

typedef struct vgp_power {
    bool  dpms_enabled;
    int   dpms_timeout_min;    /* minutes before screen off */
    float idle_seconds;
    bool  screen_off;
} vgp_power_t;

void vgp_power_init(vgp_power_t *power, int dpms_timeout_min);
void vgp_power_tick(vgp_power_t *power, float dt, struct vgp_server *server);
void vgp_power_input_activity(vgp_power_t *power, struct vgp_server *server);

#endif /* VGP_POWER_H */
