#include "launcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

static void str_to_lower(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    for (; src[i] && i < max - 1; i++)
        dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = '\0';
}

/* Remove %f, %F, %u, %U, etc. field codes from Exec= value */
static void clean_field_codes(char *s)
{
    char *src = s, *dst = s;
    while (*src) {
        if (*src == '%' && src[1] && strchr("fFuUdDnNick", src[1])) {
            src += 2;
            if (*src == ' ') src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    while (dst > s && isspace((unsigned char)dst[-1]))
        *--dst = '\0';
}

static int scan_directory(launcher_app_list_t *list, const char *dir_path)
{
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;

    struct dirent *entry;
    int added = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (list->count >= LAUNCHER_MAX_APPS)
            break;

        size_t name_len = strlen(entry->d_name);
        if (name_len < 9 || strcmp(entry->d_name + name_len - 8, ".desktop") != 0)
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        FILE *f = fopen(path, "r");
        if (!f) continue;

        launcher_app_t app;
        memset(&app, 0, sizeof(app));

        bool in_desktop_entry = false;
        bool hidden = false;
        bool no_display = false;
        bool is_application = false;
        char line[1024];

        while (fgets(line, sizeof(line), f)) {
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                line[--len] = '\0';

            if (line[0] == '[') {
                in_desktop_entry = strcmp(line, "[Desktop Entry]") == 0;
                continue;
            }

            if (!in_desktop_entry) continue;

            char *eq = strchr(line, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = line;
            char *val = eq + 1;

            if (strcmp(key, "Name") == 0 && app.name[0] == '\0')
                snprintf(app.name, sizeof(app.name), "%s", val);
            else if (strcmp(key, "Exec") == 0)
                snprintf(app.exec, sizeof(app.exec), "%s", val);
            else if (strcmp(key, "Type") == 0)
                is_application = strcmp(val, "Application") == 0;
            else if (strcmp(key, "Hidden") == 0)
                hidden = strcmp(val, "true") == 0;
            else if (strcmp(key, "NoDisplay") == 0)
                no_display = strcmp(val, "true") == 0;
            else if (strcmp(key, "Terminal") == 0)
                app.terminal = strcmp(val, "true") == 0;
        }

        fclose(f);

        if (!is_application || hidden || no_display || !app.name[0] || !app.exec[0])
            continue;

        clean_field_codes(app.exec);
        str_to_lower(app.name_lower, app.name, sizeof(app.name_lower));

        list->apps[list->count++] = app;
        added++;
    }

    closedir(dir);
    return added;
}

int launcher_scan_apps(launcher_app_list_t *list)
{
    list->count = 0;

    char path[1024];

    const char *data_home = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    if (data_home) {
        snprintf(path, sizeof(path), "%s/applications", data_home);
        scan_directory(list, path);
    } else if (home) {
        snprintf(path, sizeof(path), "%s/.local/share/applications", home);
        scan_directory(list, path);
    }

    const char *data_dirs = getenv("XDG_DATA_DIRS");
    if (!data_dirs) data_dirs = "/usr/local/share:/usr/share";

    char dirs_buf[2048];
    snprintf(dirs_buf, sizeof(dirs_buf), "%s", data_dirs);
    char *saveptr;
    char *tok = strtok_r(dirs_buf, ":", &saveptr);
    while (tok) {
        snprintf(path, sizeof(path), "%s/applications", tok);
        scan_directory(list, path);
        tok = strtok_r(NULL, ":", &saveptr);
    }

    return list->count;
}

int launcher_fuzzy_score(const char *haystack, const char *needle)
{
    if (!needle[0]) return 0;

    const char *h = haystack;
    const char *n = needle;
    int score = 0;
    bool prev_matched = false;

    while (*h && *n) {
        char hc = (char)tolower((unsigned char)*h);
        char nc = (char)tolower((unsigned char)*n);

        if (hc == nc) {
            score += 1;
            if (h == haystack) score += 10;
            if (prev_matched) score += 2;
            prev_matched = true;
            n++;
        } else {
            prev_matched = false;
        }
        h++;
    }

    if (*n) return -1;
    return score;
}

void launcher_filter(launcher_t *l)
{
    l->filtered_count = 0;

    for (int i = 0; i < l->app_list.count; i++) {
        int score = launcher_fuzzy_score(l->app_list.apps[i].name_lower,
                                          l->input_buf);
        if (score >= 0) {
            l->filtered[l->filtered_count].app_index = i;
            l->filtered[l->filtered_count].score = score;
            l->filtered_count++;
        }
    }

    /* Sort by score descending (insertion sort, small N) */
    for (int i = 1; i < l->filtered_count; i++) {
        launcher_filtered_t tmp = l->filtered[i];
        int j = i - 1;
        while (j >= 0 && l->filtered[j].score < tmp.score) {
            l->filtered[j + 1] = l->filtered[j];
            j--;
        }
        l->filtered[j + 1] = tmp;
    }
}
