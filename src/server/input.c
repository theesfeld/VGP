/* SPDX-License-Identifier: MIT */
#include "input.h"
#include "server.h"
#include "vgp/log.h"

#include <libudev.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/epoll.h>
#include <errno.h>

#define TAG "input"

/* Forward declare seat functions */
struct vgp_seat;
int  vgp_seat_open_device(struct vgp_seat *s, const char *path, int *device_id);

/* libinput interface callbacks */
static int open_restricted(const char *path, int flags, void *user_data)
{
    vgp_input_t *input = user_data;
    int fd = -1;
    (void)flags;

    if (input->seat) {
        fd = vgp_seat_open_device(input->seat, path, NULL);
    } else {
        fd = open(path, flags | O_CLOEXEC);
    }

    if (fd < 0)
        VGP_LOG_ERRNO(TAG, "open_restricted %s failed", path);
    return fd;
}

static void close_restricted(int fd, void *user_data)
{
    (void)user_data;
    close(fd);
}

static const struct libinput_interface li_iface = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted,
};

int vgp_input_init(vgp_input_t *input, vgp_event_loop_t *loop, int tty_fd)
{
    memset(input, 0, sizeof(*input));

    struct udev *udev = udev_new();
    if (!udev) {
        VGP_LOG_ERROR(TAG, "udev_new failed");
        return -1;
    }

    input->li = libinput_udev_create_context(&li_iface, input, udev);
    udev_unref(udev);

    if (!input->li) {
        VGP_LOG_ERROR(TAG, "libinput_udev_create_context failed");
        return -1;
    }

    /* Assign the seat -- default to "seat0" */
    if (libinput_udev_assign_seat(input->li, "seat0") < 0) {
        VGP_LOG_ERROR(TAG, "libinput_udev_assign_seat failed");
        libinput_unref(input->li);
        input->li = NULL;
        return -1;
    }

    input->fd = libinput_get_fd(input->li);
    input->source.type = VGP_EVENT_INPUT;
    input->source.fd = input->fd;
    input->source.data = input;

    if (vgp_event_loop_add_fd(loop, input->fd, EPOLLIN, &input->source) < 0) {
        libinput_unref(input->li);
        input->li = NULL;
        return -1;
    }

    input->initialized = true;
    VGP_LOG_INFO(TAG, "libinput initialized on seat0");
    return 0;

    (void)tty_fd;
}

void vgp_input_destroy(vgp_input_t *input, vgp_event_loop_t *loop)
{
    if (!input->initialized)
        return;

    vgp_event_loop_del_fd(loop, input->fd);

    if (input->li) {
        libinput_unref(input->li);
        input->li = NULL;
    }

    input->initialized = false;
}

void vgp_input_dispatch(vgp_input_t *input, struct vgp_server *server)
{
    libinput_dispatch(input->li);

    struct libinput_event *ev;
    while ((ev = libinput_get_event(input->li)) != NULL) {
        enum libinput_event_type type = libinput_event_get_type(ev);

        switch (type) {
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard *kb_ev =
                libinput_event_get_keyboard_event(ev);
            uint32_t keycode = libinput_event_keyboard_get_key(kb_ev);
            bool pressed = libinput_event_keyboard_get_key_state(kb_ev) ==
                           LIBINPUT_KEY_STATE_PRESSED;
            vgp_server_handle_key(server, keycode, pressed);
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION: {
            struct libinput_event_pointer *ptr_ev =
                libinput_event_get_pointer_event(ev);
            double dx = libinput_event_pointer_get_dx(ptr_ev);
            double dy = libinput_event_pointer_get_dy(ptr_ev);
            vgp_server_handle_pointer_motion(server, dx, dy);
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            struct libinput_event_pointer *ptr_ev =
                libinput_event_get_pointer_event(ev);
            uint32_t button = libinput_event_pointer_get_button(ptr_ev);
            bool pressed = libinput_event_pointer_get_button_state(ptr_ev) ==
                           LIBINPUT_BUTTON_STATE_PRESSED;
            vgp_server_handle_pointer_button(server, button, pressed);
            break;
        }
        case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
        case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
        case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS: {
            struct libinput_event_pointer *ptr_ev =
                libinput_event_get_pointer_event(ev);
            double dx = 0, dy = 0;
            if (libinput_event_pointer_has_axis(ptr_ev,
                    LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL))
                dx = libinput_event_pointer_get_scroll_value(ptr_ev,
                        LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
            if (libinput_event_pointer_has_axis(ptr_ev,
                    LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
                dy = libinput_event_pointer_get_scroll_value(ptr_ev,
                        LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
            vgp_server_handle_pointer_scroll(server, dx, dy);
            break;
        }
        default:
            break;
        }

        libinput_event_destroy(ev);
    }
}