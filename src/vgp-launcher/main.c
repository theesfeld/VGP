#include "launcher.h"
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    FILE *logfile = fopen("/tmp/vgp-launcher.log", "w");
    if (logfile) {
        setvbuf(logfile, NULL, _IOLBF, 0);
        dup2(fileno(logfile), STDERR_FILENO);
        fclose(logfile);
    }

    fprintf(stderr, "vgp-launcher: starting\n");

    signal(SIGPIPE, SIG_IGN);

    launcher_t launcher;
    fprintf(stderr, "vgp-launcher: calling init\n");
    int ret = launcher_init(&launcher);
    if (ret < 0) {
        fprintf(stderr, "vgp-launcher: init failed (ret=%d)\n", ret);
        return 1;
    }

    fprintf(stderr, "vgp-launcher: init ok, window_id=%u, %d apps scanned, entering run loop\n",
            launcher.ctx.window_id, launcher.app_list.count);
    launcher_run(&launcher);
    fprintf(stderr, "vgp-launcher: run loop exited\n");
    launcher_destroy(&launcher);

    return 0;
}
