#include "launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <errno.h>
#include <xkbcommon/xkbcommon-keysyms.h>

static void on_event(vgp_connection_t *conn, const vgp_event_t *ev, void *data)
{
    launcher_t *l = data;
    (void)conn;

    if (ev->type == VGP_EVENT_KEY_PRESS) {
        switch (ev->key.keysym) {
        case XKB_KEY_Escape:
            l->running = false;
            break;
        case XKB_KEY_Return:
        case XKB_KEY_KP_Enter:
            launcher_launch_selected(l);
            l->running = false;
            break;
        case XKB_KEY_Up:
            if (l->selected_index > 0) {
                l->selected_index--;
                if (l->selected_index < l->scroll_offset)
                    l->scroll_offset = l->selected_index;
                l->dirty = true;
            }
            break;
        case XKB_KEY_Down:
            if (l->selected_index < l->filtered_count - 1) {
                l->selected_index++;
                if (l->selected_index >= l->scroll_offset + LAUNCHER_VISIBLE_ITEMS)
                    l->scroll_offset = l->selected_index - LAUNCHER_VISIBLE_ITEMS + 1;
                l->dirty = true;
            }
            break;
        case XKB_KEY_BackSpace:
            if (l->input_len > 0) {
                l->input_buf[--l->input_len] = '\0';
                launcher_filter(l);
                l->selected_index = 0;
                l->scroll_offset = 0;
                l->dirty = true;
            }
            break;
        default:
            if (ev->key.utf8_len > 0 && (unsigned char)ev->key.utf8[0] >= 0x20) {
                int space = LAUNCHER_INPUT_MAX - l->input_len - 1;
                if ((int)ev->key.utf8_len <= space) {
                    memcpy(l->input_buf + l->input_len, ev->key.utf8,
                           ev->key.utf8_len);
                    l->input_len += (int)ev->key.utf8_len;
                    l->input_buf[l->input_len] = '\0';
                    launcher_filter(l);
                    l->selected_index = 0;
                    l->scroll_offset = 0;
                    l->dirty = true;
                }
            }
            break;
        }
    } else if (ev->type == VGP_EVENT_CLOSE) {
        l->running = false;
    }
}

int launcher_init(launcher_t *l)
{
    memset(l, 0, sizeof(*l));
    l->font_size = 14.0f;
    l->padding = 12.0f;
    l->item_height = 26.0f;

    /* Load font */
    const char *font_paths[] = {
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/Hack-Regular.ttf",
        "/usr/share/fonts/liberation-mono/LiberationMono-Regular.ttf",
        "/usr/share/fonts/noto/NotoSansMono-Regular.ttf",
        NULL,
    };
    for (const char **p = font_paths; *p; p++) {
        l->font_face = plutovg_font_face_load_from_file(*p, 0);
        if (l->font_face) { fprintf(stderr, "  font: %s\n", *p); break; }
    }
    if (!l->font_face) fprintf(stderr, "  WARNING: no font found\n");

    fprintf(stderr, "  creating surface %dx%d\n", LAUNCHER_WIDTH, LAUNCHER_HEIGHT);
    l->surface = plutovg_surface_create(LAUNCHER_WIDTH, LAUNCHER_HEIGHT);
    if (!l->surface) { fprintf(stderr, "  surface create FAILED\n"); return -1; }
    l->canvas = plutovg_canvas_create(l->surface);
    if (!l->canvas) { fprintf(stderr, "  canvas create FAILED\n"); return -1; }

    fprintf(stderr, "  scanning .desktop files\n");
    launcher_scan_apps(&l->app_list);
    fprintf(stderr, "  found %d apps\n", l->app_list.count);

    /* Load launch history (frecency) */
    {
        const char *home = getenv("HOME");
        if (home) {
            char path[512];
            snprintf(path, sizeof(path), "%s/.config/vgp/launcher_history", home);
            FILE *hf = fopen(path, "r");
            if (hf) {
                char line[256];
                while (fgets(line, sizeof(line), hf)) {
                    int count = 0;
                    char name[128];
                    if (sscanf(line, "%d %127[^\n]", &count, name) == 2) {
                        for (int i = 0; i < l->app_list.count; i++) {
                            if (strcmp(l->app_list.apps[i].name, name) == 0) {
                                l->app_list.apps[i].launch_count = count;
                                break;
                            }
                        }
                    }
                }
                fclose(hf);
                fprintf(stderr, "  loaded launch history\n");
            }
        }
    }

    fprintf(stderr, "  connecting to VGP server\n");
    l->conn = vgp_connect(NULL);
    if (!l->conn) {
        fprintf(stderr, "  vgp_connect FAILED\n");
        return -1;
    }
    fprintf(stderr, "  connected\n");

    vgp_set_event_callback(l->conn, on_event, l);

    vgp_display_info_t dpy = vgp_get_display_info(l->conn);
    fprintf(stderr, "  display: %ux%u\n", dpy.width, dpy.height);
    /* Request auto-placement (-1,-1) -- compositor places on active output */
    fprintf(stderr, "  creating window (auto-placed)\n");
    l->window_id = vgp_window_create(l->conn, -1, -1,
                                       LAUNCHER_WIDTH, LAUNCHER_HEIGHT,
                                       "VGP Launcher",
                                       VGP_WINDOW_OVERRIDE);
    if (l->window_id == 0) {
        fprintf(stderr, "  window create FAILED\n");
        return -1;
    }
    fprintf(stderr, "  window created id=%u\n", l->window_id);

    launcher_filter(l);
    fprintf(stderr, "  filtered: %d results\n", l->filtered_count);

    l->running = true;
    l->dirty = true;
    return 0;
}

void launcher_destroy(launcher_t *l)
{
    if (l->window_id && l->conn)
        vgp_window_destroy(l->conn, l->window_id);
    if (l->conn)
        vgp_disconnect(l->conn);
    if (l->canvas)
        plutovg_canvas_destroy(l->canvas);
    if (l->surface)
        plutovg_surface_destroy(l->surface);
    if (l->font_face)
        plutovg_font_face_destroy(l->font_face);
}

void launcher_run(launcher_t *l)
{
    /* Initial render */
    launcher_render(l);

    while (l->running) {
        struct pollfd fds = { .fd = vgp_fd(l->conn), .events = POLLIN };
        int ret = poll(&fds, 1, 50);
        if (ret < 0 && errno != EINTR) break;

        if (ret > 0 && (fds.revents & POLLIN)) {
            if (vgp_dispatch(l->conn) < 0) break;
        }

        if (l->dirty) {
            launcher_render(l);
            l->dirty = false;
        }
    }
}

void launcher_launch_selected(launcher_t *l)
{
    if (l->selected_index < 0 || l->selected_index >= l->filtered_count)
        return;

    int app_idx = l->filtered[l->selected_index].app_index;
    launcher_app_t *app = &l->app_list.apps[app_idx];

    app->launch_count++;

    /* Save history */
    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.config/vgp/launcher_history", home);
        FILE *hf = fopen(path, "w");
        if (hf) {
            for (int i = 0; i < l->app_list.count; i++) {
                launcher_app_t *a = &l->app_list.apps[i];
                if (a->launch_count > 0)
                    fprintf(hf, "%d %s\n", a->launch_count, a->name);
            }
            fclose(hf);
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        close(vgp_fd(l->conn));
        for (int fd = 3; fd < 1024; fd++)
            close(fd);
        signal(SIGCHLD, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);
        execl("/bin/sh", "/bin/sh", "-c", app->exec, (char *)NULL);
        _exit(127);
    }
}
