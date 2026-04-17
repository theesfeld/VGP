/* SPDX-License-Identifier: MIT */
#include "seat.h"
#include "server.h"
#include "vgp/log.h"

#include <libseat.h>
#include <sys/epoll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define TAG "seat"

/* ============================================================
 * libseat callbacks
 * ============================================================ */

static void handle_enable(struct libseat *seat, void *userdata)
{
    vgp_seat_t *s = userdata;
    (void)seat;
    s->active = true;
    VGP_LOG_INFO(TAG, "seat enabled (session active)");

    if (s->server)
        vgp_server_vt_acquire(s->server);
}

static void handle_disable(struct libseat *seat, void *userdata)
{
    vgp_seat_t *s = userdata;
    s->active = false;
    VGP_LOG_INFO(TAG, "seat disabled (session inactive)");

    if (s->server)
        vgp_server_vt_release(s->server);

    libseat_disable_seat(seat);
}

static const struct libseat_seat_listener seat_listener = {
    .enable_seat = handle_enable,
    .disable_seat = handle_disable,
};

/* ============================================================
 * Lifecycle
 * ============================================================ */

int vgp_seat_init(vgp_seat_t *s, vgp_event_loop_t *loop,
                   struct vgp_server *server)
{
    memset(s, 0, sizeof(*s));
    s->server = server;
    s->seat_fd = -1;

    s->seat = libseat_open_seat(&seat_listener, s);
    if (!s->seat) {
        VGP_LOG_ERROR(TAG, "libseat_open_seat failed: %s", strerror(errno));
        return -1;
    }

    s->seat_fd = libseat_get_fd(s->seat);
    if (s->seat_fd < 0) {
        VGP_LOG_ERROR(TAG, "libseat_get_fd failed");
        libseat_close_seat(s->seat);
        s->seat = NULL;
        return -1;
    }

    s->seat_source.type = VGP_EVENT_SIGNAL; /* reuse signal type for dispatch */
    s->seat_source.fd = s->seat_fd;
    s->seat_source.data = s;

    if (vgp_event_loop_add_fd(loop, s->seat_fd, EPOLLIN, &s->seat_source) < 0) {
        libseat_close_seat(s->seat);
        s->seat = NULL;
        return -1;
    }

    /* Initial dispatch to process pending events (enable_seat) */
    libseat_dispatch(s->seat, 0);

    s->initialized = true;
    VGP_LOG_INFO(TAG, "libseat session opened (fd=%d)", s->seat_fd);
    return 0;
}

void vgp_seat_destroy(vgp_seat_t *s, vgp_event_loop_t *loop)
{
    if (!s->initialized) return;

    if (s->seat_fd >= 0)
        vgp_event_loop_del_fd(loop, s->seat_fd);

    if (s->seat)
        libseat_close_seat(s->seat);

    s->seat = NULL;
    s->initialized = false;
    VGP_LOG_INFO(TAG, "libseat session closed");
}

void vgp_seat_dispatch(vgp_seat_t *s)
{
    if (!s->seat) return;
    libseat_dispatch(s->seat, 0);
}

/* ============================================================
 * Device management
 * ============================================================ */

int vgp_seat_open_device(vgp_seat_t *s, const char *path, int *device_id)
{
    if (!s->seat) return -1;

    int fd = -1;
    int id = libseat_open_device(s->seat, path, &fd);
    if (id < 0) {
        VGP_LOG_ERROR(TAG, "libseat_open_device(%s) failed: %s",
                      path, strerror(errno));
        return -1;
    }

    if (device_id)
        *device_id = id;

    VGP_LOG_INFO(TAG, "opened device: %s (fd=%d, id=%d)", path, fd, id);
    return fd;
}

void vgp_seat_close_device(vgp_seat_t *s, int device_id)
{
    if (!s->seat) return;
    libseat_close_device(s->seat, device_id);
}