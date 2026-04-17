/* SPDX-License-Identifier: MIT */
#include "drm.h"
#include "vgp/log.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <errno.h>

#define TAG "drm"

/* Page flip completion handler */
static void page_flip_handler(int fd, unsigned int sequence,
                               unsigned int tv_sec, unsigned int tv_usec,
                               void *user_data)
{
    (void)fd; (void)sequence; (void)tv_sec; (void)tv_usec;
    vgp_drm_output_t *output = user_data;
    output->front = 1 - output->front;
    output->page_flip_pending = false;
}

/* Try to open a DRM device */
/* Forward declare seat functions */
struct vgp_seat;
int  vgp_seat_open_device(struct vgp_seat *s, const char *path, int *device_id);

static int open_drm_device(struct vgp_seat *seat, int *out_device_id)
{
    const char *paths[] = {
        "/dev/dri/card0",
        "/dev/dri/card1",
    };

    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
        int device_id = -1;
        int fd = -1;

        if (seat) {
            /* Use libseat for proper privilege management */
            fd = vgp_seat_open_device(seat, paths[i], &device_id);
        } else {
            /* Fallback: direct open (requires root or permissions) */
            fd = open(paths[i], O_RDWR | O_CLOEXEC);
        }

        if (fd >= 0) {
            drmModeRes *res = drmModeGetResources(fd);
            if (res) {
                drmModeFreeResources(res);
                if (out_device_id) *out_device_id = device_id;
                VGP_LOG_INFO(TAG, "opened %s (fd=%d, seat_id=%d)",
                             paths[i], fd, device_id);
                return fd;
            }
            close(fd);
        }
    }

    return -1;
}

int vgp_drm_backend_init(vgp_drm_backend_t *drm, vgp_event_loop_t *loop)
{
    bool saved_skip = drm->skip_dumb_buffers;
    memset(drm, 0, sizeof(*drm));
    drm->skip_dumb_buffers = saved_skip;
    drm->drm_fd = -1;

    drm->drm_fd = open_drm_device(drm->seat, &drm->drm_device_id);
    if (drm->drm_fd < 0) {
        VGP_LOG_ERROR(TAG, "no usable DRM device found");
        return -1;
    }

    /* libseat gives us master automatically. Only call SetMaster
     * if running without seat (direct/root). */
    if (!drm->seat) {
        if (drmSetMaster(drm->drm_fd) < 0 && errno != EINVAL)
            VGP_LOG_WARN(TAG, "drmSetMaster failed: %s", strerror(errno));
    }

    /* Set up event context for page flips */
    drm->ev_ctx.version = 2;
    drm->ev_ctx.page_flip_handler = page_flip_handler;

    /* Enumerate and init outputs */
    if (vgp_drm_backend_enumerate_outputs(drm) < 0) {
        VGP_LOG_ERROR(TAG, "no usable outputs found");
        close(drm->drm_fd);
        drm->drm_fd = -1;
        return -1;
    }

    /* Add DRM fd to event loop */
    drm->drm_source.type = VGP_EVENT_DRM;
    drm->drm_source.fd = drm->drm_fd;
    drm->drm_source.data = drm;

    if (vgp_event_loop_add_fd(loop, drm->drm_fd, EPOLLIN, &drm->drm_source) < 0) {
        VGP_LOG_ERROR(TAG, "failed to add DRM fd to event loop");
        /* Cleanup outputs */
        for (int i = 0; i < drm->output_count; i++) {
            vgp_drm_fb_destroy(drm->drm_fd, &drm->outputs[i].fbs[0]);
            vgp_drm_fb_destroy(drm->drm_fd, &drm->outputs[i].fbs[1]);
        }
        close(drm->drm_fd);
        drm->drm_fd = -1;
        return -1;
    }

    return 0;
}

void vgp_drm_backend_destroy(vgp_drm_backend_t *drm, vgp_event_loop_t *loop)
{
    if (drm->drm_fd < 0)
        return;

    vgp_event_loop_del_fd(loop, drm->drm_fd);

    for (int i = 0; i < drm->output_count; i++) {
        vgp_drm_output_t *output = &drm->outputs[i];

        /* Restore saved CRTC */
        if (output->saved_crtc) {
            drmModeSetCrtc(drm->drm_fd,
                           output->saved_crtc->crtc_id,
                           output->saved_crtc->buffer_id,
                           output->saved_crtc->x,
                           output->saved_crtc->y,
                           &output->connector_id, 1,
                           &output->saved_crtc->mode);
            drmModeFreeCrtc(output->saved_crtc);
            output->saved_crtc = NULL;
        }

        vgp_drm_fb_destroy(drm->drm_fd, &output->fbs[0]);
        vgp_drm_fb_destroy(drm->drm_fd, &output->fbs[1]);
    }

    close(drm->drm_fd);
    drm->drm_fd = -1;
    VGP_LOG_INFO(TAG, "DRM backend destroyed");
}

int vgp_drm_backend_enumerate_outputs(vgp_drm_backend_t *drm)
{
    drmModeRes *res = drmModeGetResources(drm->drm_fd);
    if (!res) {
        VGP_LOG_ERRNO(TAG, "drmModeGetResources failed");
        return -1;
    }

    /* Track which CRTCs are claimed */
    bool crtc_used[32] = { false };
    drm->output_count = 0;

    for (int c = 0; c < res->count_connectors && drm->output_count < VGP_MAX_OUTPUTS; c++) {
        drmModeConnector *conn = drmModeGetConnector(drm->drm_fd, res->connectors[c]);
        if (!conn)
            continue;

        if (conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
            drmModeFreeConnector(conn);
            continue;
        }

        /* Find the preferred mode (or first mode) */
        drmModeModeInfo mode = conn->modes[0];
        for (int m = 0; m < conn->count_modes; m++) {
            if (conn->modes[m].type & DRM_MODE_TYPE_PREFERRED) {
                mode = conn->modes[m];
                break;
            }
        }

        /* Find a CRTC for this connector */
        uint32_t crtc_id = 0;
        for (int e = 0; e < conn->count_encoders && crtc_id == 0; e++) {
            drmModeEncoder *enc = drmModeGetEncoder(drm->drm_fd, conn->encoders[e]);
            if (!enc)
                continue;

            for (int cr = 0; cr < res->count_crtcs; cr++) {
                if (!(enc->possible_crtcs & (1u << cr)))
                    continue;
                if (crtc_used[cr])
                    continue;

                crtc_id = res->crtcs[cr];
                crtc_used[cr] = true;
                break;
            }
            drmModeFreeEncoder(enc);
        }

        if (crtc_id == 0) {
            VGP_LOG_WARN(TAG, "no CRTC available for connector %u", conn->connector_id);
            drmModeFreeConnector(conn);
            continue;
        }

        /* Set up the output */
        vgp_drm_output_t *output = &drm->outputs[drm->output_count];
        memset(output, 0, sizeof(*output));
        output->connector_id = conn->connector_id;
        output->crtc_id = crtc_id;
        output->mode = mode;
        output->width = mode.hdisplay;
        output->height = mode.vdisplay;
        output->x = 0;
        output->y = 0;

        /* Save current CRTC for restoration */
        output->saved_crtc = drmModeGetCrtc(drm->drm_fd, crtc_id);

        if (vgp_drm_output_init(drm, output) < 0) {
            VGP_LOG_WARN(TAG, "failed to init output for connector %u",
                         conn->connector_id);
            if (output->saved_crtc)
                drmModeFreeCrtc(output->saved_crtc);
            drmModeFreeConnector(conn);
            continue;
        }

        VGP_LOG_INFO(TAG, "output %d: %ux%u@%u connector=%u crtc=%u",
                     drm->output_count, output->width, output->height,
                     mode.vrefresh, conn->connector_id, crtc_id);

        drm->output_count++;
        drmModeFreeConnector(conn);
    }

    drmModeFreeResources(res);

    if (drm->output_count == 0) {
        VGP_LOG_ERROR(TAG, "no connected outputs found");
        return -1;
    }

    return 0;
}

int vgp_drm_fb_create(int drm_fd, vgp_drm_fb_t *fb,
                       uint32_t width, uint32_t height)
{
    memset(fb, 0, sizeof(*fb));
    fb->width = width;
    fb->height = height;
    fb->bpp = 32;

    /* Create dumb buffer */
    struct drm_mode_create_dumb create = {
        .width = width,
        .height = height,
        .bpp = 32,
    };

    if (ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        VGP_LOG_ERRNO(TAG, "DRM_IOCTL_MODE_CREATE_DUMB failed");
        return -1;
    }

    fb->handle = create.handle;
    fb->stride = create.pitch;
    fb->size = create.size;

    /* Add framebuffer */
    if (drmModeAddFB(drm_fd, width, height, 24, 32, fb->stride, fb->handle, &fb->fb_id) < 0) {
        VGP_LOG_ERRNO(TAG, "drmModeAddFB failed");
        goto err_dumb;
    }

    /* Map the buffer */
    struct drm_mode_map_dumb map = { .handle = fb->handle };
    if (ioctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        VGP_LOG_ERRNO(TAG, "DRM_IOCTL_MODE_MAP_DUMB failed");
        goto err_fb;
    }

    fb->map_offset = map.offset;
    fb->map = mmap(NULL, fb->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   drm_fd, fb->map_offset);
    if (fb->map == MAP_FAILED) {
        VGP_LOG_ERRNO(TAG, "mmap failed");
        fb->map = NULL;
        goto err_fb;
    }

    /* Clear to black */
    memset(fb->map, 0, fb->size);

    return 0;

err_fb:
    drmModeRmFB(drm_fd, fb->fb_id);
    fb->fb_id = 0;
err_dumb: {
    struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
    ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    fb->handle = 0;
    return -1;
}
}

void vgp_drm_fb_destroy(int drm_fd, vgp_drm_fb_t *fb)
{
    if (fb->map) {
        munmap(fb->map, fb->size);
        fb->map = NULL;
    }
    if (fb->fb_id) {
        drmModeRmFB(drm_fd, fb->fb_id);
        fb->fb_id = 0;
    }
    if (fb->handle) {
        struct drm_mode_destroy_dumb destroy = { .handle = fb->handle };
        ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
        fb->handle = 0;
    }
}

int vgp_drm_output_init(vgp_drm_backend_t *drm, vgp_drm_output_t *output)
{
    output->front = 0;
    output->page_flip_pending = false;

    if (drm->skip_dumb_buffers) {
        /* GPU backend will create GBM surfaces and do its own modeset */
        output->active = true;
        return 0;
    }

    /* CPU path: create dumb buffers + modeset */
    if (vgp_drm_fb_create(drm->drm_fd, &output->fbs[0],
                           output->width, output->height) < 0)
        return -1;

    if (vgp_drm_fb_create(drm->drm_fd, &output->fbs[1],
                           output->width, output->height) < 0) {
        vgp_drm_fb_destroy(drm->drm_fd, &output->fbs[0]);
        return -1;
    }

    if (vgp_drm_output_modeset(drm, output) < 0) {
        vgp_drm_fb_destroy(drm->drm_fd, &output->fbs[0]);
        vgp_drm_fb_destroy(drm->drm_fd, &output->fbs[1]);
        return -1;
    }

    output->active = true;
    return 0;
}

int vgp_drm_output_modeset(vgp_drm_backend_t *drm, vgp_drm_output_t *output)
{
    if (drmModeSetCrtc(drm->drm_fd, output->crtc_id,
                        output->fbs[output->front].fb_id,
                        0, 0,
                        &output->connector_id, 1,
                        &output->mode) < 0) {
        VGP_LOG_ERRNO(TAG, "drmModeSetCrtc failed");
        return -1;
    }
    return 0;
}

int vgp_drm_output_page_flip(vgp_drm_backend_t *drm, vgp_drm_output_t *output)
{
    if (output->page_flip_pending)
        return 0;

    int back = 1 - output->front;
    if (drmModePageFlip(drm->drm_fd, output->crtc_id,
                         output->fbs[back].fb_id,
                         DRM_MODE_PAGE_FLIP_EVENT, output) < 0) {
        VGP_LOG_ERRNO(TAG, "drmModePageFlip failed");
        return -1;
    }

    output->page_flip_pending = true;
    return 0;
}

void vgp_drm_handle_event(vgp_drm_backend_t *drm)
{
    drmHandleEvent(drm->drm_fd, &drm->ev_ctx);
}

vgp_drm_fb_t *vgp_drm_output_back_buffer(vgp_drm_output_t *output)
{
    return &output->fbs[1 - output->front];
}

void vgp_drm_drop_master(vgp_drm_backend_t *drm)
{
    if (drm->drm_fd >= 0)
        drmDropMaster(drm->drm_fd);
}

void vgp_drm_set_master(vgp_drm_backend_t *drm)
{
    if (drm->drm_fd >= 0)
        drmSetMaster(drm->drm_fd);
}