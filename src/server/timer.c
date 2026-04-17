/* SPDX-License-Identifier: MIT */
#include "timer.h"
#include "vgp/log.h"

#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define TAG "timer"

int vgp_timer_create(vgp_timer_t *timer, vgp_event_loop_t *loop,
                      vgp_timer_cb_t callback, void *data)
{
    timer->fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer->fd < 0) {
        VGP_LOG_ERRNO(TAG, "timerfd_create failed");
        return -1;
    }

    timer->callback = callback;
    timer->data = data;
    timer->source.type = VGP_EVENT_TIMER;
    timer->source.fd = timer->fd;
    timer->source.data = timer;

    if (vgp_event_loop_add_fd(loop, timer->fd, EPOLLIN, &timer->source) < 0) {
        close(timer->fd);
        timer->fd = -1;
        return -1;
    }

    return 0;
}

int vgp_timer_arm_oneshot(vgp_timer_t *timer, uint64_t nsec)
{
    struct itimerspec its = {
        .it_value = {
            .tv_sec = (time_t)(nsec / 1000000000ULL),
            .tv_nsec = (long)(nsec % 1000000000ULL),
        },
        .it_interval = { 0, 0 },
    };
    if (timerfd_settime(timer->fd, 0, &its, NULL) < 0) {
        VGP_LOG_ERRNO(TAG, "timerfd_settime oneshot failed");
        return -1;
    }
    return 0;
}

int vgp_timer_arm_repeating(vgp_timer_t *timer, uint64_t interval_nsec)
{
    struct timespec ts = {
        .tv_sec = (time_t)(interval_nsec / 1000000000ULL),
        .tv_nsec = (long)(interval_nsec % 1000000000ULL),
    };
    struct itimerspec its = {
        .it_value = ts,
        .it_interval = ts,
    };
    if (timerfd_settime(timer->fd, 0, &its, NULL) < 0) {
        VGP_LOG_ERRNO(TAG, "timerfd_settime repeating failed");
        return -1;
    }
    return 0;
}

int vgp_timer_disarm(vgp_timer_t *timer)
{
    struct itimerspec its;
    memset(&its, 0, sizeof(its));
    if (timerfd_settime(timer->fd, 0, &its, NULL) < 0) {
        VGP_LOG_ERRNO(TAG, "timerfd_settime disarm failed");
        return -1;
    }
    return 0;
}

void vgp_timer_destroy(vgp_timer_t *timer, vgp_event_loop_t *loop)
{
    if (timer->fd >= 0) {
        vgp_event_loop_del_fd(loop, timer->fd);
        close(timer->fd);
        timer->fd = -1;
    }
}