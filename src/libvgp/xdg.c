#include "vgp/xdg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

/* ------------------------------------------------------------
 * Base directory resolution
 * ------------------------------------------------------------ */

static const char *home_or_null(void)
{
    const char *h = getenv("HOME");
    return (h && *h) ? h : NULL;
}

static int write_base(vgp_xdg_kind_t kind, char *buf, size_t buf_sz)
{
    const char *env = NULL;
    const char *fallback = NULL;
    switch (kind) {
    case VGP_XDG_CONFIG:
        env = getenv("XDG_CONFIG_HOME");
        fallback = "/.config";
        break;
    case VGP_XDG_DATA:
        env = getenv("XDG_DATA_HOME");
        fallback = "/.local/share";
        break;
    case VGP_XDG_STATE:
        env = getenv("XDG_STATE_HOME");
        fallback = "/.local/state";
        break;
    case VGP_XDG_CACHE:
        env = getenv("XDG_CACHE_HOME");
        fallback = "/.cache";
        break;
    case VGP_XDG_RUNTIME:
        env = getenv("XDG_RUNTIME_DIR");
        if (env && *env) {
            int n = snprintf(buf, buf_sz, "%s", env);
            return (n > 0 && (size_t)n < buf_sz) ? 1 : 0;
        }
        /* Spec doesn't define a fallback, but a sensible degraded
         * mode for headless / minimal environments: /tmp/vgp-<uid>. */
        {
            int n = snprintf(buf, buf_sz, "/tmp/vgp-%u",
                              (unsigned)geteuid());
            if (n > 0 && (size_t)n < buf_sz) {
                (void)mkdir(buf, 0700);
                return 1;
            }
            return 0;
        }
    }

    if (env && *env) {
        int n = snprintf(buf, buf_sz, "%s", env);
        return (n > 0 && (size_t)n < buf_sz) ? 1 : 0;
    }
    const char *h = home_or_null();
    if (!h) return 0;
    int n = snprintf(buf, buf_sz, "%s%s", h, fallback);
    return (n > 0 && (size_t)n < buf_sz) ? 1 : 0;
}

int vgp_xdg_mkpath(const char *dir)
{
    if (!dir || !*dir) return 0;

    char tmp[1024];
    size_t len = strlen(dir);
    if (len >= sizeof(tmp)) return 0;
    memcpy(tmp, dir, len + 1);
    if (tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return 0;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return 0;
    return 1;
}

int vgp_xdg_resolve(vgp_xdg_kind_t kind,
                      const char *subpath,
                      char *buf, size_t buf_sz)
{
    char base[768];
    if (!write_base(kind, base, sizeof(base))) return 0;

    int n = snprintf(buf, buf_sz, "%s/%s",
                      base, subpath ? subpath : "");
    if (n <= 0 || (size_t)n >= buf_sz) return 0;

    /* Ensure parent directory exists */
    char parent[1024];
    size_t plen = (size_t)n;
    if (plen >= sizeof(parent)) return 0;
    memcpy(parent, buf, plen + 1);
    char *slash = strrchr(parent, '/');
    if (slash && slash != parent) {
        *slash = '\0';
        (void)vgp_xdg_mkpath(parent);
    }
    return 1;
}

/* ------------------------------------------------------------
 * Search paths
 * ------------------------------------------------------------ */

static int file_exists(const char *path)
{
    struct stat st;
    return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

static int search_in_list(const char *list,
                            const char *subpath,
                            char *buf, size_t buf_sz)
{
    if (!list || !*list) return 0;
    const char *p = list;
    while (*p) {
        const char *end = strchr(p, ':');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 0 && len < sizeof(buf) - 1) {
            char dir[512];
            if (len < sizeof(dir) - 1) {
                memcpy(dir, p, len);
                dir[len] = '\0';
                int n = snprintf(buf, buf_sz, "%s/%s", dir, subpath);
                if (n > 0 && (size_t)n < buf_sz && file_exists(buf))
                    return 1;
            }
        }
        if (!end) break;
        p = end + 1;
    }
    return 0;
}

int vgp_xdg_find_config(const char *subpath, char *buf, size_t buf_sz)
{
    if (!subpath) return 0;

    /* 1. XDG_CONFIG_HOME */
    char home_path[1024];
    if (vgp_xdg_resolve(VGP_XDG_CONFIG, subpath,
                          home_path, sizeof(home_path)) &&
        file_exists(home_path)) {
        if (snprintf(buf, buf_sz, "%s", home_path) > 0) return 1;
    }

    /* 2. XDG_CONFIG_DIRS -> /etc/xdg */
    const char *dirs = getenv("XDG_CONFIG_DIRS");
    if (!dirs || !*dirs) dirs = "/etc/xdg";
    return search_in_list(dirs, subpath, buf, buf_sz);
}

int vgp_xdg_find_data(const char *subpath, char *buf, size_t buf_sz)
{
    if (!subpath) return 0;

    /* 1. XDG_DATA_HOME */
    char home_path[1024];
    if (vgp_xdg_resolve(VGP_XDG_DATA, subpath,
                          home_path, sizeof(home_path)) &&
        file_exists(home_path)) {
        if (snprintf(buf, buf_sz, "%s", home_path) > 0) return 1;
    }

    /* 2. XDG_DATA_DIRS -> /usr/local/share:/usr/share */
    const char *dirs = getenv("XDG_DATA_DIRS");
    if (!dirs || !*dirs) dirs = "/usr/local/share:/usr/share";
    return search_in_list(dirs, subpath, buf, buf_sz);
}
