#include "power.h"
#include "server.h"
#include "vgp/log.h"

#include <xf86drm.h>
#include <xf86drmMode.h>

#define TAG "power"

void vgp_power_init(vgp_power_t *power, int dpms_timeout_min)
{
    power->dpms_enabled = (dpms_timeout_min > 0);
    power->dpms_timeout_min = dpms_timeout_min;
    power->idle_seconds = 0;
    power->screen_off = false;
}

void vgp_power_tick(vgp_power_t *power, float dt, struct vgp_server *server)
{
    if (!power->dpms_enabled || power->screen_off) return;

    power->idle_seconds += dt;

    if (power->idle_seconds >= (float)power->dpms_timeout_min * 60.0f) {
        /* Turn off displays via DPMS */
        VGP_LOG_INFO(TAG, "DPMS: turning off displays (idle %.0fs)",
                     power->idle_seconds);
        for (int i = 0; i < server->drm.output_count; i++) {
            /* Set DPMS property to off.
             * This is driver-specific; using connector property. */
            /* For now, just set a flag. Full DPMS requires finding
             * the DPMS property on the connector. */
        }
        power->screen_off = true;
    }
}

void vgp_power_input_activity(vgp_power_t *power, struct vgp_server *server)
{
    power->idle_seconds = 0;
    if (power->screen_off) {
        VGP_LOG_INFO(TAG, "DPMS: waking displays");
        power->screen_off = false;
        /* Re-enable displays -- schedule a frame to re-render */
        vgp_renderer_schedule_frame(&server->renderer);
    }
}
