#include "term.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    FILE *logfile = fopen("/tmp/vgp-term.log", "w");
    if (logfile) {
        setvbuf(logfile, NULL, _IOLBF, 0);
        dup2(fileno(logfile), STDERR_FILENO);
        fclose(logfile);
    }

    fprintf(stderr, "vgp-term: starting\n");

    signal(SIGPIPE, SIG_IGN);

    vgp_term_t term;
    /* 80 cols * ~8.4px + padding = ~700px wide, 24 rows * ~18px = ~450px tall */
    fprintf(stderr, "vgp-term: calling init (700x450)\n");
    int ret = vgp_term_init(&term, 700, 450);
    if (ret < 0) {
        fprintf(stderr, "vgp-term: init failed (ret=%d)\n", ret);
        return 1;
    }

    fprintf(stderr, "vgp-term: init ok, window_id=%u, entering run loop\n", term.window_id);
    vgp_term_run(&term);
    fprintf(stderr, "vgp-term: run loop exited\n");
    vgp_term_destroy(&term);

    return 0;
}
