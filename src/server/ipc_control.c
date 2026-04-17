/* SPDX-License-Identifier: MIT */
#include "ipc_control.h"
#include "server.h"
#include "spawn.h"
#include "lockscreen.h"
#include "tiling.h"
#include "vgp/log.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#define TAG "ctl"

int vgp_ipc_control_init(vgp_ipc_control_t *ctl, vgp_event_loop_t *loop)
{
    memset(ctl, 0, sizeof(*ctl));
    ctl->listen_fd = -1;

    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) runtime_dir = "/tmp";
    snprintf(ctl->socket_path, sizeof(ctl->socket_path), "%s/vgp-ctl", runtime_dir);
    unlink(ctl->socket_path);

    ctl->listen_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (ctl->listen_fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, ctl->socket_path, sizeof(addr.sun_path) - 1);

    if (bind(ctl->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(ctl->listen_fd);
        ctl->listen_fd = -1;
        return -1;
    }
    listen(ctl->listen_fd, 4);

    ctl->listen_source.type = VGP_EVENT_IPC_LISTEN;
    ctl->listen_source.fd = ctl->listen_fd;
    ctl->listen_source.data = ctl;
    vgp_event_loop_add_fd(loop, ctl->listen_fd, EPOLLIN, &ctl->listen_source);

    ctl->initialized = true;
    VGP_LOG_INFO(TAG, "control socket: %s", ctl->socket_path);
    return 0;
}

void vgp_ipc_control_destroy(vgp_ipc_control_t *ctl, vgp_event_loop_t *loop)
{
    if (!ctl->initialized) return;
    if (ctl->listen_fd >= 0) {
        vgp_event_loop_del_fd(loop, ctl->listen_fd);
        close(ctl->listen_fd);
        unlink(ctl->socket_path);
    }
    ctl->initialized = false;
}

static void send_response(int fd, const char *response)
{
    write(fd, response, strlen(response));
    write(fd, "\n", 1);
}

static void handle_command(vgp_server_t *server, int client_fd, char *cmd)
{
    /* Trim */
    size_t len = strlen(cmd);
    while (len > 0 && (cmd[len-1] == '\n' || cmd[len-1] == '\r'))
        cmd[--len] = '\0';

    VGP_LOG_DEBUG(TAG, "command: %s", cmd);

    if (strcmp(cmd, "get workspaces") == 0) {
        char buf[1024] = "[";
        for (int i = 0; i < server->compositor.output_count; i++) {
            char entry[128];
            snprintf(entry, sizeof(entry),
                     "%s{\"output\":%d,\"workspace\":%d,\"width\":%u,\"height\":%u}",
                     i > 0 ? "," : "",
                     i, server->compositor.outputs[i].workspace,
                     server->compositor.outputs[i].width,
                     server->compositor.outputs[i].height);
            strncat(buf, entry, sizeof(buf) - strlen(buf) - 1);
        }
        strncat(buf, "]", sizeof(buf) - strlen(buf) - 1);
        send_response(client_fd, buf);
    }
    else if (strcmp(cmd, "get windows") == 0) {
        char buf[4096] = "[";
        bool first = true;
        for (int i = 0; i < VGP_MAX_WINDOWS; i++) {
            vgp_window_t *w = &server->compositor.windows[i];
            if (!w->used) continue;
            char entry[256];
            snprintf(entry, sizeof(entry),
                     "%s{\"id\":%u,\"title\":\"%s\",\"workspace\":%d,"
                     "\"x\":%d,\"y\":%d,\"w\":%d,\"h\":%d,"
                     "\"focused\":%s,\"floating\":%s}",
                     first ? "" : ",",
                     w->id, w->title, w->workspace,
                     w->frame_rect.x, w->frame_rect.y,
                     w->frame_rect.w, w->frame_rect.h,
                     w->focused ? "true" : "false",
                     w->floating_override ? "true" : "false");
            strncat(buf, entry, sizeof(buf) - strlen(buf) - 1);
            first = false;
        }
        strncat(buf, "]", sizeof(buf) - strlen(buf) - 1);
        send_response(client_fd, buf);
    }
    else if (strncmp(cmd, "exec ", 5) == 0) {
        vgp_spawn(server, cmd + 5);
        send_response(client_fd, "ok");
    }
    else if (strncmp(cmd, "workspace ", 10) == 0) {
        int ws = atoi(cmd + 10);
        int out = server->compositor.active_output;
        if (out >= 0 && out < server->compositor.output_count) {
            server->compositor.outputs[out].workspace = ws;
            vgp_renderer_schedule_frame(&server->renderer);
        }
        send_response(client_fd, "ok");
    }
    else if (strcmp(cmd, "lock") == 0) {
        vgp_lockscreen_lock(&server->lockscreen);
        vgp_renderer_schedule_frame(&server->renderer);
        send_response(client_fd, "ok");
    }
    else if (strcmp(cmd, "reload") == 0) {
        vgp_config_load(&server->config, NULL);
        send_response(client_fd, "ok");
    }
    else if (strncmp(cmd, "focus ", 6) == 0) {
        uint32_t id = (uint32_t)atoi(cmd + 6);
        if (id > 0 && id <= VGP_MAX_WINDOWS) {
            vgp_window_t *w = &server->compositor.windows[id - 1];
            if (w->used) {
                vgp_compositor_focus_window(&server->compositor, w);
                vgp_renderer_schedule_frame(&server->renderer);
            }
        }
        send_response(client_fd, "ok");
    }
    else {
        send_response(client_fd, "error: unknown command");
    }
}

void vgp_ipc_control_handle(vgp_ipc_control_t *ctl, struct vgp_server *server)
{
    int client_fd = accept4(ctl->listen_fd, NULL, NULL, SOCK_CLOEXEC);
    if (client_fd < 0) return;

    /* Read command (blocking, short timeout) */
    char buf[1024];
    ssize_t n = read(client_fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        handle_command(server, client_fd, buf);
    }

    close(client_fd);
}