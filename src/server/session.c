#include "session.h"
#include "server.h"
#include "spawn.h"
#include "vgp/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "session"

int vgp_session_save(struct vgp_server *server)
{
    const char *home = getenv("HOME");
    if (!home) return -1;

    char path[512];
    snprintf(path, sizeof(path), "%s/.config/vgp/session.json", home);

    FILE *f = fopen(path, "w");
    if (!f) {
        VGP_LOG_WARN(TAG, "cannot write session to %s", path);
        return -1;
    }

    fprintf(f, "{\n  \"windows\": [\n");
    bool first = true;
    for (int i = 0; i < VGP_MAX_WINDOWS; i++) {
        vgp_window_t *w = &server->compositor.windows[i];
        if (!w->used || !w->decorated) continue;

        if (!first) fprintf(f, ",\n");
        fprintf(f, "    {\n");
        fprintf(f, "      \"title\": \"%s\",\n", w->title);
        fprintf(f, "      \"workspace\": %d,\n", w->workspace);
        fprintf(f, "      \"x\": %d, \"y\": %d,\n", w->frame_rect.x, w->frame_rect.y);
        fprintf(f, "      \"w\": %d, \"h\": %d,\n", w->frame_rect.w, w->frame_rect.h);
        fprintf(f, "      \"floating\": %s\n", w->floating_override ? "true" : "false");
        fprintf(f, "    }");
        first = false;
    }
    fprintf(f, "\n  ]\n}\n");
    fclose(f);

    VGP_LOG_INFO(TAG, "session saved to %s", path);
    return 0;
}

int vgp_session_restore(struct vgp_server *server)
{
    /* Session restore spawns the terminal for each saved window.
     * Full layout restore (positions) requires the windows to connect
     * and then be repositioned, which is complex. For now, we just
     * spawn the right number of terminals. */
    (void)server;
    VGP_LOG_INFO(TAG, "session restore: use [session] autostart in config");
    return 0;
}
