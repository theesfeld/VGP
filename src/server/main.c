#include "server.h"
#include "spawn.h"
#include "vgp/log.h"
#include "vgp/xdg.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#define TAG "main"

static vgp_server_t server;

static void signal_handler(int sig)
{
    (void)sig;
    vgp_event_loop_stop(&server.loop);
}

static void sigchld_handler(int sig)
{
    (void)sig;
    vgp_spawn_reap_children();
}

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s [OPTIONS]\n", prog);
    fprintf(stderr, "  --config PATH   Config file path (default: $XDG_CONFIG_HOME/vgp/config.toml)\n");
    fprintf(stderr, "  --help          Show this help\n");
}

int main(int argc, char *argv[])
{
    const char *config_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    /* Redirect logs to $XDG_STATE_HOME/vgp/vgp.log per the XDG Base
     * Directory Specification. Logs are state, not config. */
    char log_path[512];
    if (!vgp_xdg_resolve(VGP_XDG_STATE, "vgp/vgp.log",
                           log_path, sizeof(log_path)))
        snprintf(log_path, sizeof(log_path), "/tmp/vgp-%d.log", getuid());
    FILE *logfile = fopen(log_path, "w");
    if (logfile) {
        setvbuf(logfile, NULL, _IOLBF, 0);
        dup2(fileno(logfile), STDERR_FILENO);
        fclose(logfile);
    }

    VGP_LOG_INFO(TAG, "VGP - Vector Graphics Protocol v0.1.0");

    struct sigaction sa = { .sa_handler = signal_handler, .sa_flags = 0 };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct sigaction sa_chld = {
        .sa_handler = sigchld_handler,
        .sa_flags = SA_NOCLDSTOP | SA_RESTART,
    };
    sigemptyset(&sa_chld.sa_mask);
    sigaction(SIGCHLD, &sa_chld, NULL);

    signal(SIGPIPE, SIG_IGN);

    if (vgp_server_init(&server, config_path) < 0) {
        VGP_LOG_ERROR(TAG, "failed to initialize server");
        return 1;
    }

    vgp_server_run(&server);
    vgp_server_shutdown(&server);

    VGP_LOG_INFO(TAG, "goodbye");
    return 0;
}
