/* SPDX-License-Identifier: MIT */
#include "power.h"
#include "server.h"
#include "vgp/log.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <string.h>

#define TAG "power"

void vgp_power_init(vgp_power_t *power, int dpms_timeout_min)
{
    power->dpms_enabled = (dpms_timeout_min > 0);
    power->dpms_timeout_min = dpms_timeout_min;
    power->idle_seconds = 0;
    power->screen_off = false;
}

/* Find the DPMS property on a connector and set it */
static void set_dpms(int drm_fd, uint32_t connector_id, int mode)
{
    drmModeConnectorPtr conn = drmModeGetConnector(drm_fd, connector_id);
    if (!conn) return;

    for (int i = 0; i < conn->count_props; i++) {
        drmModePropertyPtr prop = drmModeGetProperty(drm_fd, conn->props[i]);
        if (!prop) continue;
        if (strcmp(prop->name, "DPMS") == 0) {
            drmModeConnectorSetProperty(drm_fd, connector_id,
                                         prop->prop_id, (uint64_t)mode);
            VGP_LOG_INFO(TAG, "DPMS property set to %d on connector %u",
                         mode, connector_id);
            drmModeFreeProperty(prop);
            drmModeFreeConnector(conn);
            return;
        }
        drmModeFreeProperty(prop);
    }
    drmModeFreeConnector(conn);
}

void vgp_power_tick(vgp_power_t *power, float dt, struct vgp_server *server)
{
    if (!power->dpms_enabled || power->screen_off) return;

    power->idle_seconds += dt;

    if (power->idle_seconds >= (float)power->dpms_timeout_min * 60.0f) {
        VGP_LOG_INFO(TAG, "DPMS: turning off displays (idle %.0fs)",
                     power->idle_seconds);

        /* DRM_MODE_DPMS_OFF = 3 (standby=1, suspend=2, off=3) */
        for (int i = 0; i < server->drm.output_count; i++) {
            set_dpms(server->drm.drm_fd,
                     server->drm.outputs[i].connector_id, 3);
        }
        power->screen_off = true;
    }
}

void vgp_power_input_activity(vgp_power_t *power, struct vgp_server *server)
{
    power->idle_seconds = 0;
    if (power->screen_off) {
        VGP_LOG_INFO(TAG, "DPMS: waking displays");

        /* DRM_MODE_DPMS_ON = 0 */
        for (int i = 0; i < server->drm.output_count; i++) {
            set_dpms(server->drm.drm_fd,
                     server->drm.outputs[i].connector_id, 0);
        }
        power->screen_off = false;
        vgp_renderer_schedule_frame(&server->renderer);
    }
}