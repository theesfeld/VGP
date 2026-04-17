/* SPDX-License-Identifier: MIT */
#ifndef VGP_SERVER_H
#define VGP_SERVER_H

#include "loop.h"
#include "timer.h"
#include "seat.h"
#include "vt.h"
#include "drm.h"
#include "input.h"
#include "keyboard.h"
#include "cursor.h"
#include "renderer.h"
#include "compositor.h"
#include "config.h"
#include "keybind.h"
#include "ipc.h"
#include "notify.h"
#include "animation.h"
#include "lockscreen.h"
#include "menu.h"
#include "ipc_control.h"
#include "session.h"
#include "power.h"
#include "hotreload.h"
#include "calendar.h"
#include "panel.h"
#include "arena.h"

#include "vgp/log.h"
#include "vgp/protocol.h"

typedef struct vgp_server {
    vgp_event_loop_t       loop;
    vgp_seat_t             seat;
    vgp_vt_t               vt;
    vgp_drm_backend_t      drm;
    vgp_input_t            input;
    vgp_keyboard_t         keyboard;
    vgp_renderer_t         renderer;
    vgp_compositor_t       compositor;
    vgp_config_t           config;
    vgp_keybind_manager_t  keybinds;
    vgp_ipc_t              ipc;
    vgp_arena_t            frame_arena;
    vgp_timer_t            statusbar_timer;
    vgp_timer_t            animation_timer;
    vgp_notify_t           notify;
    vgp_animation_mgr_t   animations;
    vgp_lockscreen_t      lockscreen;
    vgp_menu_t            desktop_menu;
    vgp_menu_t            window_menu;
    vgp_window_t         *menu_target_win;  /* window the menu is for */
    vgp_ipc_control_t     ctl;
    vgp_power_t           power;
    vgp_hotreload_t       hotreload;
    vgp_calendar_t        calendar;
    vgp_session_t         session;

    /* Clipboard */
    char                  *clipboard_data;
    size_t                 clipboard_len;

    bool                   running;
} vgp_server_t;

/* Lifecycle */
int  vgp_server_init(vgp_server_t *server, const char *config_path);
void vgp_server_run(vgp_server_t *server);
void vgp_server_shutdown(vgp_server_t *server);

/* Event handlers (called from event loop) */
void vgp_server_handle_signal(vgp_server_t *server);
void vgp_server_handle_drm(vgp_server_t *server);
void vgp_server_handle_input(vgp_server_t *server);
void vgp_server_handle_ipc_accept(vgp_server_t *server);
void vgp_server_handle_ipc_client(vgp_server_t *server, void *client_data);

/* Input event handlers */
void vgp_server_handle_key(vgp_server_t *server, uint32_t keycode, bool pressed);
void vgp_server_handle_pointer_motion(vgp_server_t *server, double dx, double dy);
void vgp_server_handle_pointer_button(vgp_server_t *server,
                                       uint32_t button, bool pressed);
void vgp_server_handle_pointer_scroll(vgp_server_t *server,
                                       double dx, double dy);

/* IPC */
void vgp_server_handle_message(vgp_server_t *server,
                                struct vgp_ipc_client *client,
                                vgp_msg_header_t *hdr);
void vgp_server_handle_client_disconnect(vgp_server_t *server,
                                          struct vgp_ipc_client *client);

/* VT switch */
void vgp_server_vt_release(vgp_server_t *server);
void vgp_server_vt_acquire(vgp_server_t *server);

/* Send configure event to a window's client */
void vgp_server_send_configure(vgp_server_t *server, vgp_window_t *win);

/* Frame rendering */
void vgp_server_render_frame(vgp_server_t *server);

#endif /* VGP_SERVER_H */