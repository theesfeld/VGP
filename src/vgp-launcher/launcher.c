#include "launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

int launcher_init(launcher_t *l)
{
    memset(l, 0, sizeof(*l));

    launcher_scan_apps(&l->app_list);
    fprintf(stderr, "launcher: found %d apps\n", l->app_list.count);

    /* Load launch history */
    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.config/vgp/launcher_history", home);
        FILE *hf = fopen(path, "r");
        if (hf) {
            char line[256];
            while (fgets(line, sizeof(line), hf)) {
                int count = 0; char name[128];
                if (sscanf(line, "%d %127[^\n]", &count, name) == 2) {
                    for (int i = 0; i < l->app_list.count; i++)
                        if (strcmp(l->app_list.apps[i].name, name) == 0)
                            { l->app_list.apps[i].launch_count = count; break; }
                }
            }
            fclose(hf);
        }
    }

    if (vgfx_init(&l->ctx, "VGP Launcher", LAUNCHER_WIDTH, LAUNCHER_HEIGHT,
                    VGP_WINDOW_OVERRIDE) < 0)
        return -1;

    launcher_filter(l);
    return 0;
}

void launcher_destroy(launcher_t *l)
{
    vgfx_destroy(&l->ctx);
}

void launcher_run(launcher_t *l)
{
    while (l->ctx.running) {
        vgfx_poll(&l->ctx, 16);

        /* Handle keyboard input */
        if (l->ctx.key_pressed) {
            uint32_t ks = l->ctx.last_keysym;
            if (ks == 0xFF1B) { l->ctx.running = false; } /* Escape */
            else if (ks == 0xFF0D) { launcher_launch_selected(l); l->ctx.running = false; }
            else if (ks == 0xFF52 && l->selected_index > 0) { l->selected_index--; l->ctx.dirty = true; }
            else if (ks == 0xFF54 && l->selected_index < l->filtered_count - 1) {
                l->selected_index++;
                if (l->selected_index >= l->scroll_offset + LAUNCHER_VISIBLE_ITEMS)
                    l->scroll_offset = l->selected_index - LAUNCHER_VISIBLE_ITEMS + 1;
                l->ctx.dirty = true;
            }
            else if (ks == 0xFF08 && l->input_len > 0) {
                l->input_buf[--l->input_len] = '\0';
                launcher_filter(l); l->selected_index = 0; l->scroll_offset = 0;
                l->ctx.dirty = true;
            }
            else if (l->ctx.last_utf8[0] >= 0x20 && l->input_len < LAUNCHER_INPUT_MAX - 1) {
                l->input_buf[l->input_len++] = l->ctx.last_utf8[0];
                l->input_buf[l->input_len] = '\0';
                launcher_filter(l); l->selected_index = 0; l->scroll_offset = 0;
                l->ctx.dirty = true;
            }
        }

        if (l->ctx.dirty) {
            vgfx_begin_frame(&l->ctx);
            launcher_render(l);
            vgfx_end_frame(&l->ctx);
            l->ctx.dirty = false;
        }
    }
}

void launcher_launch_selected(launcher_t *l)
{
    if (l->selected_index < 0 || l->selected_index >= l->filtered_count) return;
    int app_idx = l->filtered[l->selected_index].app_index;
    launcher_app_t *app = &l->app_list.apps[app_idx];

    app->launch_count++;
    const char *home = getenv("HOME");
    if (home) {
        char path[512];
        snprintf(path, sizeof(path), "%s/.config/vgp/launcher_history", home);
        FILE *hf = fopen(path, "w");
        if (hf) {
            for (int i = 0; i < l->app_list.count; i++) {
                launcher_app_t *a = &l->app_list.apps[i];
                if (a->launch_count > 0) fprintf(hf, "%d %s\n", a->launch_count, a->name);
            }
            fclose(hf);
        }
    }

    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        close(vgp_fd(l->ctx.conn));
        for (int fd = 3; fd < 1024; fd++) close(fd);
        signal(SIGCHLD, SIG_DFL); signal(SIGPIPE, SIG_DFL);
        sigset_t mask; sigemptyset(&mask); sigprocmask(SIG_SETMASK, &mask, NULL);
        execl("/bin/sh", "/bin/sh", "-c", app->exec, (char *)NULL);
        _exit(127);
    }
}
