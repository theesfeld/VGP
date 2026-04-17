/* SPDX-License-Identifier: MIT */
#include "term.h"

#include <pty.h>
#include <utmp.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int vgp_term_pty_spawn(vgp_term_t *term)
{
    struct winsize ws = {
        .ws_row = (unsigned short)term->rows,
        .ws_col = (unsigned short)term->cols,
    };

    pid_t pid = forkpty(&term->pty_fd, NULL, NULL, &ws);
    if (pid < 0)
        return -1;

    if (pid == 0) {
        /* Child: exec shell */
        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        setenv("VGP_TERM", "1", 1);

        const char *shell = getenv("SHELL");
        if (!shell) shell = "/bin/sh";

        /* Reset signals */
        signal(SIGCHLD, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);

        execlp(shell, shell, "-l", NULL);
        _exit(127);
    }

    /* Parent */
    term->child_pid = pid;

    /* Set master fd non-blocking */
    int flags = fcntl(term->pty_fd, F_GETFL);
    fcntl(term->pty_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

int vgp_term_pty_resize(vgp_term_t *term, int rows, int cols)
{
    struct winsize ws = {
        .ws_row = (unsigned short)rows,
        .ws_col = (unsigned short)cols,
    };
    if (ioctl(term->pty_fd, TIOCSWINSZ, &ws) < 0)
        return -1;
    return 0;
}

ssize_t vgp_term_pty_write(vgp_term_t *term, const char *data, size_t len)
{
    ssize_t total = 0;
    while ((size_t)total < len) {
        ssize_t n = write(term->pty_fd, data + total, len - (size_t)total);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN) break;
            return -1;
        }
        total += n;
    }
    return total;
}