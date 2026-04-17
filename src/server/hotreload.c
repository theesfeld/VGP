/* SPDX-License-Identifier: MIT */
#include "hotreload.h"
#include "server.h"
#include "vgp/log.h"

#include <sys/inotify.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define TAG "hotreload"
#define EVENT_BUF_SIZE 4096

int vgp_hotreload_init(vgp_hotreload_t *hr, vgp_event_loop_t *loop,
                         const char *theme_dir, const char *shader_dir)
{
    memset(hr, 0, sizeof(*hr));
    hr->inotify_fd = -1;
    hr->theme_wd = -1;
    hr->shader_wd = -1;

    hr->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (hr->inotify_fd < 0) {
        VGP_LOG_WARN(TAG, "inotify_init failed: %s", strerror(errno));
        return -1;
    }

    if (theme_dir && theme_dir[0]) {
        hr->theme_wd = inotify_add_watch(hr->inotify_fd, theme_dir,
                                           IN_MODIFY | IN_CLOSE_WRITE);
        if (hr->theme_wd >= 0)
            VGP_LOG_INFO(TAG, "watching theme dir: %s", theme_dir);
    }

    if (shader_dir && shader_dir[0]) {
        hr->shader_wd = inotify_add_watch(hr->inotify_fd, shader_dir,
                                            IN_MODIFY | IN_CLOSE_WRITE);
        if (hr->shader_wd >= 0)
            VGP_LOG_INFO(TAG, "watching shader dir: %s", shader_dir);
    }

    hr->source.type = VGP_EVENT_TIMER; /* reuse for dispatch */
    hr->source.fd = hr->inotify_fd;
    hr->source.data = hr;
    vgp_event_loop_add_fd(loop, hr->inotify_fd, EPOLLIN, &hr->source);

    hr->initialized = true;
    return 0;
}

void vgp_hotreload_destroy(vgp_hotreload_t *hr, vgp_event_loop_t *loop)
{
    if (!hr->initialized) return;
    vgp_event_loop_del_fd(loop, hr->inotify_fd);
    if (hr->theme_wd >= 0) inotify_rm_watch(hr->inotify_fd, hr->theme_wd);
    if (hr->shader_wd >= 0) inotify_rm_watch(hr->inotify_fd, hr->shader_wd);
    close(hr->inotify_fd);
    hr->initialized = false;
}

void vgp_hotreload_dispatch(vgp_hotreload_t *hr, struct vgp_server *server)
{
    if (!hr->initialized) return;

    char buf[EVENT_BUF_SIZE];
    ssize_t n = read(hr->inotify_fd, buf, sizeof(buf));
    if (n <= 0) return;

    bool theme_changed = false;
    bool shader_changed = false;

    ssize_t offset = 0;
    while (offset < n) {
        struct inotify_event *ev = (struct inotify_event *)(buf + offset);
        if (ev->wd == hr->theme_wd) theme_changed = true;
        if (ev->wd == hr->shader_wd) shader_changed = true;
        offset += (ssize_t)(sizeof(struct inotify_event) + ev->len);
    }

    if (theme_changed) {
        VGP_LOG_INFO(TAG, "theme changed, reloading config + broadcasting to clients");
        vgp_config_load(&server->config, server->config.config_path);
        /* Broadcast updated theme to all connected clients */
        extern void vgp_server_broadcast_theme(struct vgp_server *server);
        vgp_server_broadcast_theme(server);
        vgp_renderer_schedule_frame(&server->renderer);
    }

    if (shader_changed) {
        VGP_LOG_INFO(TAG, "shader changed, scheduling re-render");
        /* Shader reloading would require re-compiling the GL program.
         * For now, just trigger a re-render. Full shader hot-reload
         * requires the shader_loader to track file paths and recompile. */
        vgp_renderer_schedule_frame(&server->renderer);
    }
}