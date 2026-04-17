/* SPDX-License-Identifier: MIT */
#ifndef VGP_DRM_H
#define VGP_DRM_H

#include "loop.h"
#include "vgp/types.h"

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct vgp_drm_fb {
    uint32_t fb_id;
    uint32_t handle;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t bpp;
    uint64_t size;
    uint64_t map_offset;
    uint8_t *map;
} vgp_drm_fb_t;

typedef struct vgp_drm_output {
    uint32_t        connector_id;
    uint32_t        encoder_id;
    uint32_t        crtc_id;
    drmModeModeInfo mode;
    drmModeCrtc    *saved_crtc;

    vgp_drm_fb_t   fbs[2];
    int             front;
    bool            page_flip_pending;

    int32_t         x, y;
    uint32_t        width, height;
    bool            active;
} vgp_drm_output_t;

/* Forward declaration */
struct vgp_seat;

typedef struct vgp_drm_backend {
    int                drm_fd;
    int                drm_device_id;  /* libseat device id for cleanup */
    vgp_drm_output_t   outputs[VGP_MAX_OUTPUTS];
    int                output_count;
    drmEventContext    ev_ctx;
    vgp_event_source_t drm_source;
    bool               skip_dumb_buffers;
    struct vgp_seat   *seat;           /* seat manager for device access */
} vgp_drm_backend_t;

int  vgp_drm_backend_init(vgp_drm_backend_t *drm, vgp_event_loop_t *loop);
void vgp_drm_backend_destroy(vgp_drm_backend_t *drm, vgp_event_loop_t *loop);
int  vgp_drm_backend_enumerate_outputs(vgp_drm_backend_t *drm);

int  vgp_drm_fb_create(int drm_fd, vgp_drm_fb_t *fb,
                        uint32_t width, uint32_t height);
void vgp_drm_fb_destroy(int drm_fd, vgp_drm_fb_t *fb);

int  vgp_drm_output_init(vgp_drm_backend_t *drm, vgp_drm_output_t *output);
int  vgp_drm_output_modeset(vgp_drm_backend_t *drm, vgp_drm_output_t *output);
int  vgp_drm_output_page_flip(vgp_drm_backend_t *drm, vgp_drm_output_t *output);
void vgp_drm_handle_event(vgp_drm_backend_t *drm);

/* Get the back buffer for rendering */
vgp_drm_fb_t *vgp_drm_output_back_buffer(vgp_drm_output_t *output);

/* Drop/set DRM master for VT switching */
void vgp_drm_drop_master(vgp_drm_backend_t *drm);
void vgp_drm_set_master(vgp_drm_backend_t *drm);

#endif /* VGP_DRM_H */