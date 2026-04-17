/* SPDX-License-Identifier: MIT */
#include "vt.h"
#include "server.h"
#include "vgp/log.h"

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/signalfd.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <errno.h>

#define TAG "vt"

int vgp_vt_init(vgp_vt_t *vt, vgp_event_loop_t *loop)
{
    vt->tty_fd = -1;
    vt->signal_fd = -1;
    vt->active = false;
    vt->vt_num = 0;

    /* Open controlling tty to query for a free VT */
    int tty0_fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (tty0_fd < 0) {
        /* Fallback: try current tty */
        tty0_fd = open("/dev/tty", O_RDWR | O_CLOEXEC);
        if (tty0_fd < 0) {
            VGP_LOG_ERRNO(TAG, "cannot open /dev/tty0 or /dev/tty");
            return -1;
        }
    }

    /* Find a free VT */
    int vt_num = 0;
    if (ioctl(tty0_fd, VT_OPENQRY, &vt_num) < 0 || vt_num <= 0) {
        VGP_LOG_ERRNO(TAG, "VT_OPENQRY failed");
        close(tty0_fd);
        return -1;
    }
    close(tty0_fd);

    /* Open the VT */
    char tty_path[32];
    snprintf(tty_path, sizeof(tty_path), "/dev/tty%d", vt_num);
    vt->tty_fd = open(tty_path, O_RDWR | O_NOCTTY | O_CLOEXEC);
    if (vt->tty_fd < 0) {
        VGP_LOG_ERRNO(TAG, "cannot open %s", tty_path);
        return -1;
    }
    vt->vt_num = vt_num;

    /* Activate the VT */
    if (ioctl(vt->tty_fd, VT_ACTIVATE, vt_num) < 0) {
        VGP_LOG_ERRNO(TAG, "VT_ACTIVATE failed");
        goto err;
    }
    if (ioctl(vt->tty_fd, VT_WAITACTIVE, vt_num) < 0) {
        VGP_LOG_ERRNO(TAG, "VT_WAITACTIVE failed");
        goto err;
    }

    VGP_LOG_INFO(TAG, "acquired VT%d (%s)", vt_num, tty_path);

    /* Save and set keyboard mode */
    if (ioctl(vt->tty_fd, KDGKBMODE, &vt->saved_kb_mode) < 0) {
        VGP_LOG_ERRNO(TAG, "KDGKBMODE failed");
        goto err;
    }
    if (ioctl(vt->tty_fd, KDSKBMODE, K_OFF) < 0) {
        VGP_LOG_ERRNO(TAG, "KDSKBMODE K_OFF failed");
        goto err;
    }

    /* Set graphics mode */
    if (ioctl(vt->tty_fd, KDSETMODE, KD_GRAPHICS) < 0) {
        VGP_LOG_ERRNO(TAG, "KDSETMODE KD_GRAPHICS failed");
        goto err;
    }

    /* Set up process-controlled VT switching */
    struct vt_mode vtm = {
        .mode = VT_PROCESS,
        .relsig = SIGUSR1,
        .acqsig = SIGUSR2,
    };
    if (ioctl(vt->tty_fd, VT_SETMODE, &vtm) < 0) {
        VGP_LOG_ERRNO(TAG, "VT_SETMODE failed");
        goto err_mode;
    }

    /* Block SIGUSR1/SIGUSR2 and create signalfd */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    vt->signal_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (vt->signal_fd < 0) {
        VGP_LOG_ERRNO(TAG, "signalfd failed");
        goto err_mode;
    }

    vt->signal_source.type = VGP_EVENT_SIGNAL;
    vt->signal_source.fd = vt->signal_fd;
    vt->signal_source.data = vt;

    if (vgp_event_loop_add_fd(loop, vt->signal_fd, EPOLLIN, &vt->signal_source) < 0)
        goto err_sigfd;

    vt->active = true;
    return 0;

err_sigfd:
    close(vt->signal_fd);
    vt->signal_fd = -1;
err_mode:
    ioctl(vt->tty_fd, KDSETMODE, KD_TEXT);
    ioctl(vt->tty_fd, KDSKBMODE, vt->saved_kb_mode);
err:
    close(vt->tty_fd);
    vt->tty_fd = -1;
    return -1;
}

void vgp_vt_destroy(vgp_vt_t *vt, vgp_event_loop_t *loop)
{
    if (vt->signal_fd >= 0) {
        vgp_event_loop_del_fd(loop, vt->signal_fd);
        close(vt->signal_fd);
        vt->signal_fd = -1;
    }

    if (vt->tty_fd >= 0) {
        /* Restore VT to automatic mode */
        struct vt_mode vtm = { .mode = VT_AUTO };
        ioctl(vt->tty_fd, VT_SETMODE, &vtm);

        /* Restore keyboard and text mode */
        ioctl(vt->tty_fd, KDSKBMODE, vt->saved_kb_mode);
        ioctl(vt->tty_fd, KDSETMODE, KD_TEXT);

        close(vt->tty_fd);
        vt->tty_fd = -1;
    }

    VGP_LOG_INFO(TAG, "released VT%d", vt->vt_num);
}

void vgp_vt_handle_signal(vgp_vt_t *vt, struct vgp_server *server)
{
    struct signalfd_siginfo si;
    ssize_t n = read(vt->signal_fd, &si, sizeof(si));
    if (n != sizeof(si))
        return;

    if (si.ssi_signo == SIGUSR1) {
        /* Release: another VT wants to become active */
        VGP_LOG_INFO(TAG, "VT release requested");
        vt->active = false;
        vgp_server_vt_release(server);
        ioctl(vt->tty_fd, VT_RELDISP, 1);
    } else if (si.ssi_signo == SIGUSR2) {
        /* Acquire: we're becoming active again */
        VGP_LOG_INFO(TAG, "VT acquired");
        vt->active = true;
        vgp_server_vt_acquire(server);
    }
}