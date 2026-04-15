#include "spawn.h"
#include "server.h"
#include "vgp/log.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <linux/limits.h>

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
