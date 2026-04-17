#include "session.h"
#include "server.h"
#include "spawn.h"
#include "vgp/log.h"
#include "vgp/xdg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TAG "session"

int vgp_session_save(struct vgp_server *server)
{
    char path[512];
    if (!vgp_xdg_resolve(VGP_XDG_STATE, "vgp/session.json",
                           path, sizeof(path)))
        return -1;

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

    VGP_LOG_INFO(TAG, "session saved (%d windows)", server->compositor.window_count);
    return 0;
}

/* Simple JSON string value extraction */
static int json_string_value(const char *json, const char *key, char *out, int max)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return -1;
    p = strchr(p + strlen(search), '"');
    if (!p) return -1;
    p++; /* skip opening quote */
    int i = 0;
    while (*p && *p != '"' && i < max - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return 0;
}

static int json_int_value(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0;
    p += strlen(search);
    while (*p && isspace((unsigned char)*p)) p++;
    return atoi(p);
}

static bool json_bool_value(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p += strlen(search);
    while (*p && isspace((unsigned char)*p)) p++;
    return (strncmp(p, "true", 4) == 0);
}

int vgp_session_load(vgp_session_t *session)
{
    memset(session, 0, sizeof(*session));

    char path[512];
    if (!vgp_xdg_resolve(VGP_XDG_STATE, "vgp/session.json",
                           path, sizeof(path)))
        return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || size > 1024 * 1024) { fclose(f); return -1; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);

    /* Parse each window entry (simple brace-matching) */
    const char *p = buf;
    while (session->count < VGP_SESSION_MAX_ENTRIES) {
        /* Find next '{' inside the windows array */
        const char *start = strchr(p, '{');
        if (!start) break;
        /* Skip the outer '{' */
        if (p == buf) { p = start + 1; start = strchr(p, '{'); if (!start) break; }

        const char *end = strchr(start, '}');
        if (!end) break;

        /* Extract this entry */
        int entry_len = (int)(end - start + 1);
        char entry[1024];
        if (entry_len >= (int)sizeof(entry)) { p = end + 1; continue; }
        memcpy(entry, start, (size_t)entry_len);
        entry[entry_len] = '\0';

        vgp_session_entry_t *e = &session->entries[session->count];
        json_string_value(entry, "title", e->title, (int)sizeof(e->title));
        e->workspace = json_int_value(entry, "workspace");
        e->rect.x = json_int_value(entry, "x");
        e->rect.y = json_int_value(entry, "y");
        e->rect.w = json_int_value(entry, "w");
        e->rect.h = json_int_value(entry, "h");
        e->floating = json_bool_value(entry, "floating");
        e->matched = false;

        if (e->title[0]) session->count++;
        p = end + 1;
    }

    free(buf);

    if (session->count > 0) {
        session->restoring = true;
        VGP_LOG_INFO(TAG, "session loaded: %d windows", session->count);
    }
    return session->count > 0 ? 0 : -1;
}

bool vgp_session_match_window(vgp_session_t *session, const char *title,
                               vgp_rect_t *rect_out, int *workspace_out,
                               bool *floating_out)
{
    if (!session->restoring || !title || !title[0])
        return false;

    /* Find first unmatched entry with a matching title prefix */
    for (int i = 0; i < session->count; i++) {
        vgp_session_entry_t *e = &session->entries[i];
        if (e->matched) continue;

        /* Match by title prefix (window titles may change slightly) */
        if (strncmp(e->title, title, strlen(title)) == 0 ||
            strncmp(title, e->title, strlen(e->title)) == 0 ||
            strstr(e->title, title) || strstr(title, e->title)) {
            e->matched = true;
            *rect_out = e->rect;
            *workspace_out = e->workspace;
            *floating_out = e->floating;

            VGP_LOG_INFO(TAG, "session matched '%s' -> ws %d @ %d,%d %dx%d",
                         title, e->workspace, e->rect.x, e->rect.y,
                         e->rect.w, e->rect.h);

            /* Check if all entries are matched */
            bool all_matched = true;
            for (int j = 0; j < session->count; j++) {
                if (!session->entries[j].matched) { all_matched = false; break; }
            }
            if (all_matched) session->restoring = false;

            return true;
        }
    }
    return false;
}
