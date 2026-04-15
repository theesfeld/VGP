#include "loop.h"
#include "server.h"
#include "timer.h"
#include "vgp/log.h"

#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

#define TAG "loop"
#define MAX_EVENTS 32

int vgp_event_loop_init(vgp_event_loop_t *loop)
{
    loop->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (loop->epoll_fd < 0) {
        VGP_LOG_ERRNO(TAG, "epoll_create1 failed");
        return -1;
    }
    loop->running = false;
    return 0;
}

void vgp_event_loop_destroy(vgp_event_loop_t *loop)
{
    if (loop->epoll_fd >= 0) {
        close(loop->epoll_fd);
        loop->epoll_fd = -1;
    }
}

int vgp_event_loop_add_fd(vgp_event_loop_t *loop, int fd,
                           uint32_t events, vgp_event_source_t *source)
{
    struct epoll_event ev = {
        .events = events,
        .data.ptr = source,
    };
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
        VGP_LOG_ERRNO(TAG, "epoll_ctl ADD fd=%d failed", fd);
        return -1;
    }
    return 0;
}

int vgp_event_loop_mod_fd(vgp_event_loop_t *loop, int fd,
                           uint32_t events, vgp_event_source_t *source)
{
    struct epoll_event ev = {
        .events = events,
        .data.ptr = source,
    };
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        VGP_LOG_ERRNO(TAG, "epoll_ctl MOD fd=%d failed", fd);
        return -1;
    }
    return 0;
}

int vgp_event_loop_del_fd(vgp_event_loop_t *loop, int fd)
{
    if (epoll_ctl(loop->epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) {
        VGP_LOG_ERRNO(TAG, "epoll_ctl DEL fd=%d failed", fd);
        return -1;
    }
    return 0;
}

void vgp_event_loop_run(vgp_event_loop_t *loop, struct vgp_server *server)
{
    struct epoll_event events[MAX_EVENTS];
    loop->running = true;

    VGP_LOG_INFO(TAG, "entering main loop");

    while (loop->running) {
        int n = epoll_wait(loop->epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            VGP_LOG_ERRNO(TAG, "epoll_wait failed");
            break;
        }

        for (int i = 0; i < n; i++) {
            vgp_event_source_t *src = events[i].data.ptr;
            if (!src)
                continue;

            switch (src->type) {
            case VGP_EVENT_SIGNAL:
                vgp_server_handle_signal(server);
                break;
            case VGP_EVENT_DRM:
                vgp_server_handle_drm(server);
                break;
            case VGP_EVENT_INPUT:
                vgp_server_handle_input(server);
                break;
            case VGP_EVENT_IPC_LISTEN:
                vgp_server_handle_ipc_accept(server);
                break;
            case VGP_EVENT_IPC_CLIENT:
                vgp_server_handle_ipc_client(server, src->data);
                break;
            case VGP_EVENT_TIMER: {
                vgp_timer_t *timer = src->data;
                uint64_t expirations;
                if (read(timer->fd, &expirations, sizeof(expirations)) > 0) {
                    timer->callback(timer->data);
                }
                break;
            }
            }
        }
    }

    VGP_LOG_INFO(TAG, "exited main loop");
}

void vgp_event_loop_stop(vgp_event_loop_t *loop)
{
    loop->running = false;
}
