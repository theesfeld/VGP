/* SPDX-License-Identifier: MIT */
#include "spawn.h"
#include "server.h"
#include "vgp/log.h"
#include "vgp/xdg.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <stdbool.h>

#define TAG "spawn"

/* Get the directory containing the running vgp binary */
static void get_exe_dir(char *buf, size_t len)
{
    ssize_t n = readlink("/proc/self/exe", buf, len - 1);
    if (n <= 0) { buf[0] = '\0'; return; }
    buf[n] = '\0';
    /* Strip the binary name to get the directory */
    char *slash = strrchr(buf, '/');
    if (slash) *slash = '\0';
    else buf[0] = '\0';
}

int vgp_spawn(struct vgp_server *server, const char *cmd)
{
    if (!cmd || cmd[0] == '\0') {
        VGP_LOG_WARN(TAG, "empty command");
        return -1;
    }

    VGP_LOG_INFO(TAG, "spawning: %s", cmd);

    pid_t pid = fork();
    if (pid < 0) {
        VGP_LOG_ERRNO(TAG, "fork failed");
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        setsid();

        /* Redirect stdin from /dev/null */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) {
            dup2(devnull, STDIN_FILENO);
            if (devnull > STDERR_FILENO)
                close(devnull);
        }

        /* Close all server FDs (they have O_CLOEXEC but be safe) */
        for (int fd = STDERR_FILENO + 1; fd < 1024; fd++)
            close(fd);

        /* Prepend the server's binary directory to PATH so sibling
         * binaries (vgp-term, vgp-launcher) are found */
        char exe_dir[PATH_MAX];
        get_exe_dir(exe_dir, sizeof(exe_dir));
        if (exe_dir[0]) {
            const char *old_path = getenv("PATH");
            char new_path[PATH_MAX * 2];
            snprintf(new_path, sizeof(new_path), "%s%s%s",
                     exe_dir, old_path ? ":" : "", old_path ? old_path : "");
            setenv("PATH", new_path, 1);
        }

        /* Set environment for VGP session */
        setenv("VGP_DISPLAY", server->ipc.socket_path, 1);

        /* Unset DISPLAY -- we are NOT X11. Programs that check $DISPLAY
         * for X11 availability should find it absent and use alternative
         * paths (D-Bus, direct IPC, etc.) */
        unsetenv("DISPLAY");
        unsetenv("WAYLAND_DISPLAY");

        /* Ensure D-Bus session bus is available */
        if (!getenv("DBUS_SESSION_BUS_ADDRESS")) {
            char dbus_path[256];
            snprintf(dbus_path, sizeof(dbus_path),
                     "/run/user/%d/bus", getuid());
            struct stat st;
            if (stat(dbus_path, &st) == 0) {
                char dbus_addr[300];
                snprintf(dbus_addr, sizeof(dbus_addr),
                         "unix:path=%s", dbus_path);
                setenv("DBUS_SESSION_BUS_ADDRESS", dbus_addr, 1);
            }
        }

        /* Set TERM for terminal programs */
        if (!getenv("TERM"))
            setenv("TERM", "xterm-256color", 0);

        /* Reset signals */
        signal(SIGCHLD, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGPIPE, SIG_DFL);

        /* Unblock all signals */
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        execl("/bin/sh", "/bin/sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    return (int)pid;
}

void vgp_spawn_reap_children(void)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

/* ============================================================
 * XDG Autostart Spec
 * https://specifications.freedesktop.org/autostart-spec/latest/
 * ============================================================ */

/* Parse one .desktop file and return its command line, or NULL if the
 * entry should be skipped (Hidden=true, Type!=Application, no command,
 * parse failure). Caller frees. */
static char *parse_desktop_command(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    char line[1024];
    char *cmd = NULL;
    bool in_desktop_entry = false;
    bool hidden = false;
    bool is_application = true;

    while (fgets(line, sizeof(line), f)) {
        char *s = line;
        while (*s == ' ' || *s == '\t') s++;
        size_t len = strlen(s);
        while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' ||
                             s[len - 1] == ' '  || s[len - 1] == '\t'))
            s[--len] = '\0';
        if (!*s || *s == '#') continue;

        if (*s == '[') {
            in_desktop_entry = (strcmp(s, "[Desktop Entry]") == 0);
            continue;
        }
        if (!in_desktop_entry) continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = s, *v = eq + 1;
        while (*v == ' ') v++;

        if (strcmp(k, "Hidden") == 0 && strcmp(v, "true") == 0)
            hidden = true;
        else if (strcmp(k, "Type") == 0)
            is_application = (strcmp(v, "Application") == 0);
        else if (strcmp(k, "Exec") == 0 && !cmd)
            cmd = strdup(v);
    }
    fclose(f);

    if (hidden || !is_application || !cmd) {
        free(cmd);
        return NULL;
    }
    return cmd;
}

/* Strip field codes per the Desktop Entry Spec. */
static void strip_field_codes(char *s)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r == '%' && r[1]) {
            char c = r[1];
            if (c == '%') { *w++ = '%'; r += 2; continue; }
            if (c == 'f' || c == 'F' || c == 'u' || c == 'U' ||
                c == 'i' || c == 'c' || c == 'k' || c == 'd' ||
                c == 'D' || c == 'n' || c == 'N' || c == 'v' ||
                c == 'm') { r += 2; continue; }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

#define SEEN_MAX 256
typedef struct {
    char names[SEEN_MAX][128];
    int  count;
} seen_t;

static bool seen_contains(const seen_t *s, const char *name)
{
    for (int i = 0; i < s->count; i++)
        if (strcmp(s->names[i], name) == 0) return true;
    return false;
}

static void seen_add(seen_t *s, const char *name)
{
    if (s->count < SEEN_MAX)
        snprintf(s->names[s->count++], sizeof(s->names[0]), "%s", name);
}

static void scan_autostart_dir(struct vgp_server *server,
                                 const char *dir, seen_t *seen)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        const char *name = e->d_name;
        size_t nlen = strlen(name);
        if (nlen < 9) continue;
        if (strcmp(name + nlen - 8, ".desktop") != 0) continue;
        if (seen_contains(seen, name)) continue;
        seen_add(seen, name);

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        char *cmd = parse_desktop_command(path);
        if (!cmd) continue;
        strip_field_codes(cmd);
        VGP_LOG_INFO(TAG, "xdg-autostart: %s -> %s", name, cmd);
        vgp_spawn(server, cmd);
        free(cmd);
    }
    closedir(d);
}

void vgp_autostart_xdg(struct vgp_server *server)
{
    seen_t seen = { .count = 0 };

    /* Priority 1: $XDG_CONFIG_HOME/autostart */
    char path[PATH_MAX];
    if (vgp_xdg_resolve(VGP_XDG_CONFIG, "autostart",
                          path, sizeof(path)))
        scan_autostart_dir(server, path, &seen);

    /* Priority 2: each XDG_CONFIG_DIRS entry in order. A filename
     * already seen at a higher priority is skipped. */
    const char *dirs = getenv("XDG_CONFIG_DIRS");
    if (!dirs || !*dirs) dirs = "/etc/xdg";
    const char *p = dirs;
    while (*p) {
        const char *end = strchr(p, ':');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > 0 && len < sizeof(path) - 16) {
            char base[PATH_MAX];
            memcpy(base, p, len);
            base[len] = '\0';
            snprintf(path, sizeof(path), "%s/autostart", base);
            scan_autostart_dir(server, path, &seen);
        }
        if (!end) break;
        p = end + 1;
    }
}