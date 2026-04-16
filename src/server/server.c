#include "server.h"
#include "spawn.h"
#include "tiling.h"
#include "vgp/log.h"
#include "vgp/protocol.h"

#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input-event-codes.h>

#define TAG "server"

/* Global log level */
vgp_log_level_t vgp_log_level = VGP_LOG_LEVEL_DEBUG;

/* Send WINDOW_CONFIGURE to a window's client when geometry changes */
void vgp_server_send_configure(vgp_server_t *server, vgp_window_t *win)
{
    if (!win || win->client_fd < 0) return;
    vgp_ipc_client_t *client = vgp_ipc_find_client(&server->ipc, win->client_fd);
    if (!client) return;

    vgp_msg_window_configure_t msg = {
        .header = {
            .magic = VGP_PROTOCOL_MAGIC,
            .type = VGP_MSG_WINDOW_CONFIGURE,
            .length = sizeof(msg),
            .window_id = win->id,
        },
        .x = win->content_rect.x,
        .y = win->content_rect.y,
        .width = (uint32_t)win->content_rect.w,
        .height = (uint32_t)win->content_rect.h,
    };
    vgp_ipc_send(client, &msg, sizeof(msg));
}

/* Desktop menu action callbacks */
static void menu_action_terminal(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    vgp_spawn(s, s->config.general.terminal_cmd);
}
static void menu_action_files(void *srv, int idx) {
    (void)idx; vgp_spawn(srv, "vgp-files");
}
static void menu_action_settings(void *srv, int idx) {
    (void)idx; vgp_spawn(srv, "vgp-settings");
}
static void menu_action_lock(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    vgp_lockscreen_lock(&s->lockscreen);
    vgp_renderer_schedule_frame(&s->renderer);
}
static void menu_action_quit(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    vgp_event_loop_stop(&s->loop);
}

/* Window menu action callbacks */
static void wmenu_close(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    if (s->menu_target_win && s->menu_target_win->used)
        vgp_compositor_destroy_window(&s->compositor, s->menu_target_win);
    s->menu_target_win = NULL;
    vgp_renderer_schedule_frame(&s->renderer);
}
static void wmenu_minimize(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    if (s->menu_target_win && s->menu_target_win->used)
        vgp_compositor_minimize_window(&s->compositor, s->menu_target_win);
    s->menu_target_win = NULL;
    vgp_renderer_schedule_frame(&s->renderer);
}
static void wmenu_maximize(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    if (s->menu_target_win && s->menu_target_win->used && s->drm.output_count > 0)
        vgp_compositor_maximize_window(&s->compositor, s->menu_target_win,
            s->drm.outputs[0].width, s->drm.outputs[0].height, &s->config.theme);
    s->menu_target_win = NULL;
    vgp_renderer_schedule_frame(&s->renderer);
}
static void wmenu_toggle_float(void *srv, int idx) {
    vgp_server_t *s = srv; (void)idx;
    if (s->menu_target_win && s->menu_target_win->used)
        s->menu_target_win->floating_override = !s->menu_target_win->floating_override;
    s->menu_target_win = NULL;
    vgp_renderer_schedule_frame(&s->renderer);
}

/* Re-tile a workspace if in tiling mode, and notify all affected clients */
static void server_retile(vgp_server_t *server, int workspace)
{
    if (strcmp(server->config.general.wm_mode, "floating") == 0) return;
    vgp_tile_config_t tc = {
        .algorithm = vgp_tile_parse_algorithm(server->config.general.tile_algorithm),
        .master_ratio = server->config.general.tile_master_ratio,
        .gap_inner = server->config.general.tile_gap_inner,
        .gap_outer = server->config.general.tile_gap_outer,
        .smart_gaps = server->config.general.tile_smart_gaps,
    };
    vgp_compositor_retile(&server->compositor, workspace, &tc, &server->config.theme);

    /* Send WINDOW_CONFIGURE to all windows that were tiled so they can
     * adjust their grid dimensions (terminal cols/rows, app layout) */
    for (int i = 0; i < VGP_MAX_WINDOWS; i++) {
        vgp_window_t *w = &server->compositor.windows[i];
        if (w->used && w->visible && w->workspace == workspace &&
            w->decorated && !w->floating_override) {
            vgp_server_send_configure(server, w);
        }
    }
}

static void statusbar_tick(void *data)
{
    vgp_server_t *server = data;
    /* Always schedule a frame -- shaders need continuous animation */
    vgp_renderer_schedule_frame(&server->renderer);
}

/* High-frequency tick for smooth shader animation (30fps minimum) */
static void animation_tick(void *data)
{
    vgp_server_t *server = data;
    vgp_renderer_schedule_frame(&server->renderer);
}

int vgp_server_init(vgp_server_t *server, const char *config_path)
{
    memset(server, 0, sizeof(*server));

    VGP_LOG_INFO(TAG, "initializing VGP server");

    /* 1. Event loop */
    if (vgp_event_loop_init(&server->loop) < 0)
        return -1;

    /* 2. Seat management (libseat -- handles VT + device access via logind) */
    if (vgp_seat_init(&server->seat, &server->loop, server) < 0) {
        VGP_LOG_WARN(TAG, "libseat failed, falling back to direct VT access (needs root)");
        /* Fall through -- VT init will try direct access */
    }

    /* 3. VT acquisition (if seat didn't handle it) */
    if (!server->seat.initialized) {
        if (vgp_vt_init(&server->vt, &server->loop) < 0)
            goto err_loop;
    }

    /* 4. DRM/KMS backend */
    server->drm.seat = server->seat.initialized ? &server->seat : NULL;

    /* Always try GPU first -- skip dumb buffers if GPU backend is compiled in */
    {
#ifdef VGP_HAS_GPU_BACKEND
        const char *force_cpu = getenv("VGP_CPU");
        if (!(force_cpu && force_cpu[0] == '1')) {
            server->drm.skip_dumb_buffers = true;
            VGP_LOG_INFO(TAG, "GPU backend available, skipping dumb buffers");
        }
#endif
    }
    if (vgp_drm_backend_init(&server->drm, &server->loop) < 0)
        goto err_seat;

    /* 4. Config (loads theme + keybind entries + general/input settings) */
    vgp_config_load(&server->config, config_path);

    /* 6. Input */
    server->input.seat = server->seat.initialized ? &server->seat : NULL;
    if (vgp_input_init(&server->input, &server->loop, server->vt.tty_fd) < 0)
        goto err_drm;

    /* 6. Keyboard */
    if (vgp_keyboard_init(&server->keyboard) < 0)
        goto err_input;

    /* 7. Keybinds (resolve config entries to keysym+action) */
    vgp_keybind_init(&server->keybinds, &server->config);

    /* 8. Compositor */
    if (vgp_compositor_init(&server->compositor) < 0)
        goto err_keyboard;

    /* Set up output layout in compositor, applying monitor config */
    {
        uint32_t widths[VGP_MAX_OUTPUTS], heights[VGP_MAX_OUTPUTS];
        for (int i = 0; i < server->drm.output_count; i++) {
            widths[i] = server->drm.outputs[i].width;
            heights[i] = server->drm.outputs[i].height;
        }
        vgp_compositor_set_outputs(&server->compositor,
                                    server->drm.output_count,
                                    widths, heights);

        /* Apply per-monitor config overrides */
        for (int i = 0; i < server->drm.output_count &&
                         i < server->config.monitor_count; i++) {
            vgp_config_monitor_t *mcfg = &server->config.monitors[i];
            if (!mcfg->configured) continue;

            if (mcfg->x >= 0 || mcfg->y >= 0) {
                server->compositor.outputs[i].x = mcfg->x >= 0 ? mcfg->x : server->compositor.outputs[i].x;
                server->compositor.outputs[i].y = mcfg->y >= 0 ? mcfg->y : server->compositor.outputs[i].y;
                VGP_LOG_INFO(TAG, "monitor %d: position overridden to %d,%d",
                             i, server->compositor.outputs[i].x,
                             server->compositor.outputs[i].y);
            }
            if (mcfg->workspace >= 0) {
                server->compositor.outputs[i].workspace = mcfg->workspace;
                VGP_LOG_INFO(TAG, "monitor %d: workspace set to %d",
                             i, mcfg->workspace);
            }
        }
    }

    /* 8. Renderer */
    if (vgp_renderer_init(&server->renderer, &server->drm,
                           &server->loop, server) < 0)
        goto err_compositor;
    /* Apply accessibility settings to renderer */
    server->renderer.focus_indicator = server->config.accessibility.focus_indicator;
    server->renderer.font_scale = server->config.accessibility.font_scale;
    server->renderer.large_cursor = server->config.accessibility.large_cursor;

    /* 9. IPC */
    if (vgp_ipc_init(&server->ipc, &server->loop) < 0)
        goto err_renderer;

    /* 10. Frame arena (1MB) */
    if (vgp_arena_init(&server->frame_arena, 1024 * 1024) < 0)
        goto err_ipc;

    /* 11. Status bar timer (1-second tick for clock) */
    if (vgp_timer_create(&server->statusbar_timer, &server->loop,
                          statusbar_tick, server) < 0)
        goto err_arena;
    vgp_timer_arm_repeating(&server->statusbar_timer, VGP_MS_TO_NS(1000));

    /* 12. Animation, lock screen, desktop menu, IPC control, power */
    vgp_anim_init(&server->animations, 0.2f,
                    !server->config.accessibility.reduce_animations);
    vgp_lockscreen_init(&server->lockscreen,
                          server->config.lockscreen.enabled,
                          server->config.lockscreen.timeout_min);
    vgp_menu_init(&server->desktop_menu);
    vgp_menu_add(&server->desktop_menu, "Terminal", menu_action_terminal);
    vgp_menu_add(&server->desktop_menu, "Files", menu_action_files);
    vgp_menu_add(&server->desktop_menu, "Settings", menu_action_settings);
    vgp_menu_add_separator(&server->desktop_menu);
    vgp_menu_add(&server->desktop_menu, "Lock Screen", menu_action_lock);
    vgp_menu_add_separator(&server->desktop_menu);
    vgp_menu_add(&server->desktop_menu, "Quit VGP", menu_action_quit);
    vgp_menu_init(&server->window_menu);
    vgp_menu_add(&server->window_menu, "Close", wmenu_close);
    vgp_menu_add(&server->window_menu, "Minimize", wmenu_minimize);
    vgp_menu_add(&server->window_menu, "Maximize", wmenu_maximize);
    vgp_menu_add_separator(&server->window_menu);
    vgp_menu_add(&server->window_menu, "Toggle Float", wmenu_toggle_float);
    server->menu_target_win = NULL;

    vgp_ipc_control_init(&server->ctl, &server->loop);
    vgp_power_init(&server->power, 15);
    vgp_calendar_init(&server->calendar);

    /* Session layout restore (load saved window positions) */
    vgp_session_load(&server->session);

    /* Theme hot-reload */
    vgp_hotreload_init(&server->hotreload, &server->loop,
                         server->config.general.theme_dir, NULL);

    /* 13. Notification daemon (D-Bus) */
    vgp_notify_init(&server->notify, &server->loop);

    /* 13. Animation timer -- drives continuous shader animation (~60fps) */
    if (vgp_timer_create(&server->animation_timer, &server->loop,
                          animation_tick, server) < 0)
        goto err_arena;
    vgp_timer_arm_repeating(&server->animation_timer, VGP_MS_TO_NS(16));

    /* Schedule initial frame render */
    vgp_renderer_schedule_frame(&server->renderer);

    server->running = true;
    VGP_LOG_INFO(TAG, "VGP server initialized successfully");

    /* Auto-start programs from config */
    for (int i = 0; i < server->config.session.autostart_count; i++) {
        VGP_LOG_INFO(TAG, "autostart: %s", server->config.session.autostart[i]);
        vgp_spawn(server, server->config.session.autostart[i]);
    }

    return 0;

err_arena:
    vgp_arena_destroy(&server->frame_arena);
err_ipc:
    vgp_ipc_destroy(&server->ipc, &server->loop);
err_renderer:
    vgp_renderer_destroy(&server->renderer, &server->loop);
err_compositor:
    vgp_compositor_destroy(&server->compositor);
err_keyboard:
    vgp_keyboard_destroy(&server->keyboard);
err_input:
    vgp_input_destroy(&server->input, &server->loop);
err_drm:
    vgp_drm_backend_destroy(&server->drm, &server->loop);
err_seat:
    if (server->seat.initialized)
        vgp_seat_destroy(&server->seat, &server->loop);
    else
        vgp_vt_destroy(&server->vt, &server->loop);
err_loop:
    vgp_event_loop_destroy(&server->loop);
    return -1;
}

void vgp_server_run(vgp_server_t *server)
{
    vgp_event_loop_run(&server->loop, server);
}

void vgp_server_shutdown(vgp_server_t *server)
{
    VGP_LOG_INFO(TAG, "shutting down VGP server");

    /* Save session before shutdown */
    vgp_session_save(server);

    vgp_ipc_control_destroy(&server->ctl, &server->loop);
    vgp_notify_destroy(&server->notify, &server->loop);
    vgp_timer_destroy(&server->animation_timer, &server->loop);
    vgp_timer_destroy(&server->statusbar_timer, &server->loop);
    vgp_arena_destroy(&server->frame_arena);
    vgp_ipc_destroy(&server->ipc, &server->loop);
    vgp_renderer_destroy(&server->renderer, &server->loop);
    vgp_compositor_destroy(&server->compositor);
    vgp_keyboard_destroy(&server->keyboard);
    vgp_input_destroy(&server->input, &server->loop);
    vgp_drm_backend_destroy(&server->drm, &server->loop);
    if (server->seat.initialized)
        vgp_seat_destroy(&server->seat, &server->loop);
    else
        vgp_vt_destroy(&server->vt, &server->loop);
    vgp_event_loop_destroy(&server->loop);
}

/* ============================================================
 * Event handlers
 * ============================================================ */

void vgp_server_handle_signal(vgp_server_t *server)
{
    if (server->seat.initialized) {
        /* libseat uses this fd for enable/disable events */
        vgp_seat_dispatch(&server->seat);
    } else {
        vgp_vt_handle_signal(&server->vt, server);
    }
}

void vgp_server_handle_drm(vgp_server_t *server)
{
    vgp_drm_handle_event(&server->drm);

    /* After page flip completes, if scene is dirty, schedule another frame */
    if (server->renderer.dirty) {
        for (int i = 0; i < server->drm.output_count; i++) {
            if (!server->drm.outputs[i].page_flip_pending) {
                vgp_renderer_schedule_frame(&server->renderer);
                break;
            }
        }
    }
}

void vgp_server_handle_input(vgp_server_t *server)
{
    vgp_input_dispatch(&server->input, server);
}

void vgp_server_handle_ipc_accept(vgp_server_t *server)
{
    vgp_ipc_accept(&server->ipc, &server->loop);
}

void vgp_server_handle_ipc_client(vgp_server_t *server, void *client_data)
{
    vgp_ipc_client_t *client = client_data;
    vgp_ipc_client_dispatch(client, server);
}

/* ============================================================
 * Input event handlers
 * ============================================================ */

void vgp_server_handle_key(vgp_server_t *server, uint32_t keycode, bool pressed)
{
    vgp_key_event_t key_event;
    vgp_keyboard_process_key(&server->keyboard, keycode, pressed, &key_event);

    /* Lock screen intercepts ALL input when active */
    if (vgp_lockscreen_is_locked(&server->lockscreen)) {
        if (pressed)
            vgp_lockscreen_key(&server->lockscreen, key_event.keysym,
                                key_event.utf8, (int)key_event.utf8_len);
        vgp_renderer_schedule_frame(&server->renderer);
        return;
    }

    /* Reset idle timers on any input */
    vgp_lockscreen_input_activity(&server->lockscreen);
    vgp_power_input_activity(&server->power, server);

    /* Check keybinds BEFORE forwarding to client */
    const vgp_keybind_t *bind = vgp_keybind_match(&server->keybinds, &key_event);
    if (bind) {
        vgp_keybind_execute(server, bind);
        return; /* consumed by compositor */
    }

    /* Forward key event to focused window's client */
    vgp_window_t *focused = server->compositor.focused;
    if (focused && focused->client_fd >= 0) {
        vgp_msg_key_event_t msg = {
            .header = {
                .magic = VGP_PROTOCOL_MAGIC,
                .type = pressed ? VGP_MSG_KEY_PRESS : VGP_MSG_KEY_RELEASE,
                .flags = 0,
                .length = sizeof(vgp_msg_key_event_t),
                .window_id = focused->id,
            },
            .keycode = key_event.keycode,
            .keysym = key_event.keysym,
            .modifiers = key_event.modifiers,
            .utf8_len = key_event.utf8_len,
        };
        memcpy(msg.utf8, key_event.utf8, sizeof(msg.utf8));

        vgp_ipc_client_t *client = vgp_ipc_find_client(&server->ipc,
                                                         focused->client_fd);
        if (client)
            vgp_ipc_send(client, &msg, sizeof(msg));
    }
}

void vgp_server_handle_pointer_motion(vgp_server_t *server, double dx, double dy)
{
    vgp_lockscreen_input_activity(&server->lockscreen);
    vgp_power_input_activity(&server->power, server);
    if (vgp_lockscreen_is_locked(&server->lockscreen)) return;

    /* Total screen bounds = all outputs laid out side by side */
    uint32_t screen_w = 0, screen_h = 0;
    for (int i = 0; i < server->compositor.output_count; i++) {
        uint32_t right = (uint32_t)server->compositor.outputs[i].x +
                          server->compositor.outputs[i].width;
        uint32_t bottom = (uint32_t)server->compositor.outputs[i].y +
                           server->compositor.outputs[i].height;
        if (right > screen_w) screen_w = right;
        if (bottom > screen_h) screen_h = bottom;
    }
    if (screen_w == 0) { screen_w = 1920; screen_h = 1080; }

    vgp_cursor_t *cursor = &server->compositor.cursor;
    float speed = server->config.input.pointer_speed;
    vgp_cursor_move(cursor, (float)dx * speed, (float)dy * speed, screen_w, screen_h);

    /* Handle active grab (window move/resize) */
    vgp_grab_t *grab = &server->compositor.grab;
    if (grab->active && grab->target) {
        if (grab->region == VGP_HIT_TITLEBAR) {
            int32_t new_x = grab->grab_rect.x + (int32_t)(cursor->x - (float)grab->grab_x);
            int32_t new_y = grab->grab_rect.y + (int32_t)(cursor->y - (float)grab->grab_y);
            vgp_compositor_move_window(&server->compositor, grab->target,
                                        new_x, new_y, &server->config.theme);
        } else if (grab->region >= VGP_HIT_BORDER_N &&
                   grab->region <= VGP_HIT_BORDER_SW) {
            int32_t dx_i = (int32_t)(cursor->x - (float)grab->grab_x);
            int32_t dy_i = (int32_t)(cursor->y - (float)grab->grab_y);
            vgp_rect_t new_frame = grab->grab_rect;

            if (grab->region == VGP_HIT_BORDER_E ||
                grab->region == VGP_HIT_BORDER_NE ||
                grab->region == VGP_HIT_BORDER_SE)
                new_frame.w += dx_i;
            if (grab->region == VGP_HIT_BORDER_W ||
                grab->region == VGP_HIT_BORDER_NW ||
                grab->region == VGP_HIT_BORDER_SW) {
                new_frame.x += dx_i;
                new_frame.w -= dx_i;
            }
            if (grab->region == VGP_HIT_BORDER_S ||
                grab->region == VGP_HIT_BORDER_SE ||
                grab->region == VGP_HIT_BORDER_SW)
                new_frame.h += dy_i;
            if (grab->region == VGP_HIT_BORDER_N ||
                grab->region == VGP_HIT_BORDER_NE ||
                grab->region == VGP_HIT_BORDER_NW) {
                new_frame.y += dy_i;
                new_frame.h -= dy_i;
            }

            vgp_compositor_move_window(&server->compositor, grab->target,
                                        new_frame.x, new_frame.y, &server->config.theme);
            vgp_compositor_resize_window(&server->compositor, grab->target,
                                          (uint32_t)new_frame.w,
                                          (uint32_t)new_frame.h,
                                          &server->config.theme);
        }
    }

    /* Send mouse move to focused window's client if cursor is over content */
    vgp_window_t *focused = server->compositor.focused;
    if (focused && focused->client_fd >= 0 && !grab->active) {
        float mx = cursor->x;
        float my = cursor->y;
        vgp_rect_t *cr = &focused->content_rect;
        if (mx >= (float)cr->x && mx < (float)(cr->x + cr->w) &&
            my >= (float)cr->y && my < (float)(cr->y + cr->h)) {
            vgp_msg_mouse_move_event_t msg = {
                .header = {
                    .magic = VGP_PROTOCOL_MAGIC,
                    .type = VGP_MSG_MOUSE_MOVE,
                    .length = sizeof(msg),
                    .window_id = focused->id,
                },
                .x = mx - (float)cr->x,
                .y = my - (float)cr->y,
                .modifiers = vgp_keyboard_get_modifiers(&server->keyboard),
            };
            vgp_ipc_client_t *client =
                vgp_ipc_find_client(&server->ipc, focused->client_fd);
            if (client)
                vgp_ipc_send(client, &msg, sizeof(msg));
        }
    }

    vgp_renderer_schedule_frame(&server->renderer);
}

void vgp_server_handle_pointer_button(vgp_server_t *server,
                                       uint32_t button, bool pressed)
{
    vgp_cursor_t *cursor = &server->compositor.cursor;
    vgp_grab_t *grab = &server->compositor.grab;

    if (pressed) {
        cursor->buttons |= (1u << button);

        /* Context menu: check if any menu is visible first */
        if (server->desktop_menu.visible || server->window_menu.visible) {
            float local_mx = cursor->x;
            float local_my = cursor->y;
            int aout = vgp_compositor_output_at_cursor(&server->compositor);
            local_mx -= (float)server->compositor.outputs[aout].x;
            if (server->desktop_menu.visible &&
                vgp_menu_click(&server->desktop_menu, local_mx, local_my, server)) {
                vgp_renderer_schedule_frame(&server->renderer);
                goto button_done;
            }
            if (server->window_menu.visible &&
                vgp_menu_click(&server->window_menu, local_mx, local_my, server)) {
                vgp_renderer_schedule_frame(&server->renderer);
                goto button_done;
            }
        }

        /* Right-click: show context menu */
        if (button == 0x111 && !grab->active) { /* BTN_RIGHT */
            int32_t cx = (int32_t)cursor->x;
            int32_t cy = (int32_t)cursor->y;
            vgp_window_t *win = vgp_compositor_window_at(&server->compositor, cx, cy);
            int aout = vgp_compositor_output_at_cursor(&server->compositor);
            float local_x = cursor->x - (float)server->compositor.outputs[aout].x;
            float local_y = cursor->y;

            if (win && win->decorated) {
                vgp_hit_region_t hit = vgp_window_hit_test(win,
                    &server->config.theme, cx, cy);
                if (hit == VGP_HIT_TITLEBAR) {
                    server->menu_target_win = win;
                    vgp_menu_show(&server->window_menu, local_x, local_y);
                    vgp_renderer_schedule_frame(&server->renderer);
                    goto button_done;
                }
            }
            if (!win) {
                vgp_menu_show(&server->desktop_menu, local_x, local_y);
                vgp_renderer_schedule_frame(&server->renderer);
                goto button_done;
            }
        }

        if (!grab->active) {
            int32_t cx = (int32_t)cursor->x;
            int32_t cy = (int32_t)cursor->y;

            /* Check if click is in the panel area */
            int active_out = vgp_compositor_output_at_cursor(&server->compositor);
            vgp_output_info_t *aout = &server->compositor.outputs[active_out];
            float local_x = cursor->x - (float)aout->x;
            float local_y = cursor->y - (float)aout->y;

            if (vgp_panel_click(&server->config.panel, &server->config.theme,
                                 local_x, local_y,
                                 aout->width, aout->height,
                                 server, active_out)) {
                goto button_done;
            }

            vgp_window_t *win = vgp_compositor_window_at(&server->compositor,
                                                          cx, cy);

            if (win) {
                vgp_hit_region_t hit = vgp_window_hit_test(win, &server->config.theme,
                                                            cx, cy);
                switch (hit) {
                case VGP_HIT_TITLEBAR:
                    grab->target = win;
                    grab->region = VGP_HIT_TITLEBAR;
                    grab->grab_x = cx;
                    grab->grab_y = cy;
                    grab->grab_rect = win->frame_rect;
                    grab->active = true;
                    vgp_compositor_focus_window(&server->compositor, win);
                    break;

                case VGP_HIT_CLOSE_BTN: {
                    /* Try to notify client, but force-destroy either way */
                    if (win->client_fd >= 0) {
                        vgp_msg_header_t msg = {
                            .magic = VGP_PROTOCOL_MAGIC,
                            .type = VGP_MSG_WINDOW_CLOSE,
                            .length = sizeof(vgp_msg_header_t),
                            .window_id = win->id,
                        };
                        vgp_ipc_client_t *cl =
                            vgp_ipc_find_client(&server->ipc, win->client_fd);
                        if (cl)
                            vgp_ipc_send(cl, &msg, sizeof(msg));
                    }
                    /* Force destroy the window immediately */
                    vgp_compositor_destroy_window(&server->compositor, win);
                    vgp_renderer_schedule_frame(&server->renderer);
                }
                    break;

                case VGP_HIT_MAXIMIZE_BTN:
                    if (server->drm.output_count > 0) {
                        vgp_compositor_maximize_window(&server->compositor, win,
                                                        server->drm.outputs[0].width,
                                                        server->drm.outputs[0].height,
                                                        &server->config.theme);
                    }
                    break;

                case VGP_HIT_MINIMIZE_BTN:
                    vgp_compositor_minimize_window(&server->compositor, win);
                    break;

                case VGP_HIT_CONTENT:
                    vgp_compositor_focus_window(&server->compositor, win);
                    if (win->client_fd >= 0) {
                        vgp_msg_mouse_button_event_t msg = {
                            .header = {
                                .magic = VGP_PROTOCOL_MAGIC,
                                .type = VGP_MSG_MOUSE_BUTTON,
                                .length = sizeof(msg),
                                .window_id = win->id,
                            },
                            .x = cursor->x - (float)win->content_rect.x,
                            .y = cursor->y - (float)win->content_rect.y,
                            .button = button,
                            .state = 1,
                            .modifiers = vgp_keyboard_get_modifiers(&server->keyboard),
                        };
                        vgp_ipc_client_t *client =
                            vgp_ipc_find_client(&server->ipc, win->client_fd);
                        if (client)
                            vgp_ipc_send(client, &msg, sizeof(msg));
                    }
                    break;

                default:
                    if (hit >= VGP_HIT_BORDER_N && hit <= VGP_HIT_BORDER_SW) {
                        grab->target = win;
                        grab->region = hit;
                        grab->grab_x = (int32_t)cursor->x;
                        grab->grab_y = (int32_t)cursor->y;
                        grab->grab_rect = win->frame_rect;
                        grab->active = true;
                        vgp_compositor_focus_window(&server->compositor, win);
                    }
                    break;
                }
            }
        }
    }
button_done:
    if (!pressed) {
        cursor->buttons &= ~(1u << button);

        if (grab->active) {
            /* Send configure if window was resized via drag */
            if (grab->target && (grab->region >= VGP_HIT_BORDER_N &&
                                  grab->region <= VGP_HIT_BORDER_SW))
                vgp_server_send_configure(server, grab->target);
            grab->active = false;
            grab->target = NULL;
        }

        vgp_window_t *focused = server->compositor.focused;
        if (focused && focused->client_fd >= 0) {
            vgp_msg_mouse_button_event_t msg = {
                .header = {
                    .magic = VGP_PROTOCOL_MAGIC,
                    .type = VGP_MSG_MOUSE_BUTTON,
                    .length = sizeof(msg),
                    .window_id = focused->id,
                },
                .x = cursor->x - (float)focused->content_rect.x,
                .y = cursor->y - (float)focused->content_rect.y,
                .button = button,
                .state = 0,
                .modifiers = vgp_keyboard_get_modifiers(&server->keyboard),
            };
            vgp_ipc_client_t *client =
                vgp_ipc_find_client(&server->ipc, focused->client_fd);
            if (client)
                vgp_ipc_send(client, &msg, sizeof(msg));
        }
    }

    vgp_renderer_schedule_frame(&server->renderer);
}

void vgp_server_handle_pointer_scroll(vgp_server_t *server,
                                       double dx, double dy)
{
    vgp_window_t *focused = server->compositor.focused;
    if (focused && focused->client_fd >= 0) {
        vgp_msg_mouse_scroll_event_t msg = {
            .header = {
                .magic = VGP_PROTOCOL_MAGIC,
                .type = VGP_MSG_MOUSE_SCROLL,
                .length = sizeof(msg),
                .window_id = focused->id,
            },
            .dx = (float)dx,
            .dy = (float)dy,
            .modifiers = vgp_keyboard_get_modifiers(&server->keyboard),
        };
        vgp_ipc_client_t *client =
            vgp_ipc_find_client(&server->ipc, focused->client_fd);
        if (client)
            vgp_ipc_send(client, &msg, sizeof(msg));
    }
}

/* ============================================================
 * IPC message handler
 * ============================================================ */

void vgp_server_handle_message(vgp_server_t *server,
                                struct vgp_ipc_client *client,
                                vgp_msg_header_t *hdr)
{
    switch (hdr->type) {
    case VGP_MSG_CONNECT: {
        uint32_t w = 1920, h = 1080;
        if (server->drm.output_count > 0) {
            w = server->drm.outputs[0].width;
            h = server->drm.outputs[0].height;
        }
        vgp_msg_connect_ok_t reply = {
            .header = {
                .magic = VGP_PROTOCOL_MAGIC,
                .type = VGP_MSG_CONNECT_OK,
                .flags = VGP_MSG_FLAG_RESPONSE,
                .length = sizeof(reply),
                .window_id = 0,
            },
            .protocol_version = VGP_PROTOCOL_VERSION,
            .client_id = client->client_id,
            .display_width = w,
            .display_height = h,
        };
        vgp_ipc_send(client, &reply, sizeof(reply));
        VGP_LOG_INFO(TAG, "client %u connected (handshake complete)", client->client_id);
        break;
    }

    case VGP_MSG_WINDOW_CREATE: {
        vgp_msg_window_create_t *msg = (vgp_msg_window_create_t *)hdr;

        char title[VGP_MAX_TITLE_LEN] = "Untitled";
        if (msg->title_len > 0 && msg->title_len < VGP_MAX_TITLE_LEN) {
            char *title_data = (char *)(msg + 1);
            memcpy(title, title_data, msg->title_len);
            title[msg->title_len] = '\0';
        }

        int32_t x = msg->x, y = msg->y;
        /* Pass -1 through -- compositor auto-places on the active output */

        vgp_window_t *win = vgp_compositor_create_window(
            &server->compositor, client->fd,
            x, y, msg->width, msg->height,
            title, &server->config.theme);

        if (win) {
            /* Handle override windows (no decorations, always on top) */
            if (msg->flags & 0x0004) {
                win->decorated = false;
                /* For override windows, frame == content (no titlebar/border) */
                win->frame_rect = (vgp_rect_t){ x, y, (int32_t)msg->width, (int32_t)msg->height };
                win->content_rect = win->frame_rect;
            }
            vgp_msg_window_created_t reply = {
                .header = {
                    .magic = VGP_PROTOCOL_MAGIC,
                    .type = VGP_MSG_WINDOW_CREATED,
                    .flags = VGP_MSG_FLAG_RESPONSE,
                    .length = sizeof(reply),
                    .window_id = win->id,
                },
                .window_id = win->id,
                .x = win->content_rect.x,
                .y = win->content_rect.y,
                .width = (uint32_t)win->content_rect.w,
                .height = (uint32_t)win->content_rect.h,
            };
            vgp_ipc_send(client, &reply, sizeof(reply));

            /* Apply window rules based on title match */
            for (int ri = 0; ri < server->config.window_rule_count; ri++) {
                vgp_window_rule_t *rule = &server->config.window_rules[ri];
                if (strstr(win->title, rule->title_match)) {
                    if (rule->floating) win->floating_override = true;
                    if (rule->workspace >= 0) win->workspace = rule->workspace;
                    break;
                }
            }

            /* Session restore: match window to saved position */
            if (server->session.restoring && win->decorated) {
                vgp_rect_t sess_rect;
                int sess_ws;
                bool sess_float;
                if (vgp_session_match_window(&server->session, title,
                                              &sess_rect, &sess_ws, &sess_float)) {
                    win->frame_rect = sess_rect;
                    win->content_rect = vgp_window_content_rect(&sess_rect,
                                                                 &server->config.theme);
                    win->workspace = sess_ws;
                    if (sess_float) win->floating_override = true;
                    /* Send updated configure to client */
                    vgp_server_send_configure(server, win);
                }
            }

            /* Re-tile if in tiling mode */
            server_retile(server, win->workspace);

            /* Trigger open animation */
            vgp_anim_window_open(&server->animations, win->id,
                                  (float)win->frame_rect.x, (float)win->frame_rect.y,
                                  (float)win->frame_rect.w, (float)win->frame_rect.h);

            vgp_renderer_schedule_frame(&server->renderer);
        }
        break;
    }

    case VGP_MSG_WINDOW_DESTROY: {
        uint32_t win_id = hdr->window_id;
        if (win_id > 0 && win_id <= VGP_MAX_WINDOWS) {
            vgp_window_t *win = &server->compositor.windows[win_id - 1];
            if (win->used && win->client_fd == client->fd) {
                int ws = win->workspace;
                vgp_compositor_destroy_window(&server->compositor, win);
                server_retile(server, ws);
                vgp_renderer_schedule_frame(&server->renderer);
            }
        }
        break;
    }

    case VGP_MSG_WINDOW_SET_TITLE: {
        uint32_t win_id = hdr->window_id;
        if (win_id > 0 && win_id <= VGP_MAX_WINDOWS) {
            vgp_window_t *win = &server->compositor.windows[win_id - 1];
            if (win->used) {
                size_t title_len = hdr->length - sizeof(vgp_msg_header_t);
                if (title_len > 0 && title_len < VGP_MAX_TITLE_LEN) {
                    char *title_data = (char *)(hdr + 1);
                    memcpy(win->title, title_data, title_len);
                    win->title[title_len] = '\0';
                    vgp_renderer_schedule_frame(&server->renderer);
                }
            }
        }
        break;
    }

    case VGP_MSG_SURFACE_ATTACH: {
        vgp_msg_surface_attach_t *msg = (vgp_msg_surface_attach_t *)hdr;
        uint32_t win_id = hdr->window_id;
        if (win_id > 0 && win_id <= VGP_MAX_WINDOWS) {
            vgp_window_t *win = &server->compositor.windows[win_id - 1];
            if (win->used) {
                uint32_t w = msg->width;
                uint32_t h = msg->height;
                uint32_t stride = msg->stride;
                uint8_t *pixel_data = (uint8_t *)(msg + 1);

                VGP_LOG_DEBUG(TAG, "surface_attach: win=%u %ux%u stride=%u",
                              win_id, w, h, stride);

                if (win->client_width != w || win->client_height != h) {
                    if (win->client_surface)
                        plutovg_surface_destroy(win->client_surface);
                    win->client_surface = plutovg_surface_create((int)w, (int)h);
                    win->client_width = w;
                    win->client_height = h;
                }

                if (win->client_surface) {
                    uint8_t *dst = plutovg_surface_get_data(win->client_surface);
                    int dst_stride = plutovg_surface_get_stride(win->client_surface);
                    for (uint32_t row = 0; row < h; row++) {
                        memcpy(dst + row * dst_stride,
                               pixel_data + row * stride,
                               w * 4);
                    }
                }

                vgp_renderer_schedule_frame(&server->renderer);
            }
        }
        break;
    }

    case VGP_MSG_CLIPBOARD_SET: {
        /* Client sets clipboard content */
        size_t data_len = hdr->length - sizeof(vgp_msg_header_t);
        if (data_len > 0 && data_len < 1024 * 1024) { /* 1MB max */
            free(server->clipboard_data);
            server->clipboard_data = malloc(data_len + 1);
            if (server->clipboard_data) {
                memcpy(server->clipboard_data, (char *)(hdr + 1), data_len);
                server->clipboard_data[data_len] = '\0';
                server->clipboard_len = data_len;
            }
        }
        break;
    }

    case VGP_MSG_CLIPBOARD_GET: {
        /* Client requests clipboard content */
        size_t payload_len = server->clipboard_data ? server->clipboard_len : 0;
        size_t msg_len = sizeof(vgp_msg_header_t) + payload_len;
        uint8_t *buf = malloc(msg_len);
        if (buf) {
            vgp_msg_header_t *reply = (vgp_msg_header_t *)buf;
            reply->magic = VGP_PROTOCOL_MAGIC;
            reply->type = VGP_MSG_CLIPBOARD_DATA;
            reply->flags = VGP_MSG_FLAG_RESPONSE;
            reply->length = (uint32_t)msg_len;
            reply->window_id = 0;
            if (payload_len > 0)
                memcpy(buf + sizeof(vgp_msg_header_t),
                       server->clipboard_data, payload_len);
            vgp_ipc_send(client, buf, msg_len);
            free(buf);
        }
        break;
    }

    case VGP_MSG_SET_FONT_SIZE: {
        vgp_msg_set_font_size_t *msg = (vgp_msg_set_font_size_t *)hdr;
        uint32_t win_id = hdr->window_id;
        if (win_id > 0 && win_id <= VGP_MAX_WINDOWS) {
            vgp_window_t *win = &server->compositor.windows[win_id - 1];
            if (win->used) {
                win->font_size_override = msg->font_size;
                VGP_LOG_DEBUG(TAG, "window %u font size: %.0f", win_id, msg->font_size);
                vgp_renderer_schedule_frame(&server->renderer);
            }
        }
        break;
    }

    case VGP_MSG_CELLGRID: {
        vgp_msg_cellgrid_t *msg = (vgp_msg_cellgrid_t *)hdr;
        uint32_t win_id = hdr->window_id;
        if (win_id > 0 && win_id <= VGP_MAX_WINDOWS) {
            vgp_window_t *win = &server->compositor.windows[win_id - 1];
            if (win->used) {
                uint16_t rows = msg->rows;
                uint16_t cols = msg->cols;
                size_t cell_size = (size_t)rows * cols * sizeof(vgp_cell_t);
                void *cell_data = (uint8_t *)(msg + 1);

                /* Allocate or reallocate cell grid */
                if (win->grid_rows != rows || win->grid_cols != cols) {
                    free(win->cellgrid);
                    win->cellgrid = malloc(cell_size);
                    win->grid_rows = rows;
                    win->grid_cols = cols;
                }

                if (win->cellgrid) {
                    memcpy(win->cellgrid, cell_data, cell_size);
                    win->cursor_row = msg->cursor_row;
                    win->cursor_col = msg->cursor_col;
                    win->cursor_visible = msg->cursor_visible;
                    win->cursor_shape = msg->cursor_shape;
                    win->has_cellgrid = true;
                }

                vgp_renderer_schedule_frame(&server->renderer);
            }
        }
        break;
    }

    case VGP_MSG_DISCONNECT:
        vgp_server_handle_client_disconnect(server, client);
        break;

    default:
        VGP_LOG_DEBUG(TAG, "unhandled message type 0x%04x from client %u",
                      hdr->type, client->client_id);
        break;
    }
}

void vgp_server_handle_client_disconnect(vgp_server_t *server,
                                          struct vgp_ipc_client *client)
{
    VGP_LOG_INFO(TAG, "client %u disconnecting", client->client_id);

    for (int i = 0; i < VGP_MAX_WINDOWS; i++) {
        vgp_window_t *win = &server->compositor.windows[i];
        if (win->used && win->client_fd == client->fd)
            vgp_compositor_destroy_window(&server->compositor, win);
    }

    vgp_ipc_client_disconnect(&server->ipc, client, &server->loop);
    vgp_renderer_schedule_frame(&server->renderer);
}

/* ============================================================
 * VT switch callbacks
 * ============================================================ */

void vgp_server_vt_release(vgp_server_t *server)
{
    if (!server->seat.initialized)
        vgp_drm_drop_master(&server->drm);
    /* With libseat, master is managed automatically */
}

void vgp_server_vt_acquire(vgp_server_t *server)
{
    if (!server->seat.initialized)
        vgp_drm_set_master(&server->drm);

    for (int i = 0; i < server->drm.output_count; i++)
        vgp_drm_output_modeset(&server->drm, &server->drm.outputs[i]);

    /* Re-init output surfaces since buffers may have changed */
    if (server->renderer.backend) {
        for (int i = 0; i < server->drm.output_count; i++)
            server->renderer.backend->ops->output_init(
                server->renderer.backend, i, &server->drm.outputs[i]);
    }

    vgp_renderer_schedule_frame(&server->renderer);
}

/* ============================================================
 * Frame rendering
 * ============================================================ */

void vgp_server_render_frame(vgp_server_t *server)
{
    server->renderer.frame_scheduled = false;
    server->renderer.dirty = false;
    vgp_arena_reset(&server->frame_arena);

    /* Tick subsystems */
    vgp_notify_dispatch(&server->notify);
    vgp_notify_tick(&server->notify, 0.016f);
    vgp_anim_tick(&server->animations, 0.016f);
    vgp_lockscreen_tick(&server->lockscreen, 0.016f);
    vgp_power_tick(&server->power, 0.016f, server);

    /* Update cursor shape based on what's under it */
    {
        int32_t cx = (int32_t)server->compositor.cursor.x;
        int32_t cy = (int32_t)server->compositor.cursor.y;
        vgp_window_t *hover_win = vgp_compositor_window_at(&server->compositor, cx, cy);
        if (hover_win && !server->compositor.grab.active) {
            vgp_hit_region_t hit = vgp_window_hit_test(hover_win,
                &server->config.theme, cx, cy);
            switch (hit) {
            case VGP_HIT_BORDER_N: case VGP_HIT_BORDER_S:
                server->compositor.cursor.shape = VGP_CURSOR_RESIZE_N; break;
            case VGP_HIT_BORDER_E: case VGP_HIT_BORDER_W:
                server->compositor.cursor.shape = VGP_CURSOR_RESIZE_E; break;
            case VGP_HIT_BORDER_NE: case VGP_HIT_BORDER_SW:
                server->compositor.cursor.shape = VGP_CURSOR_RESIZE_NE; break;
            case VGP_HIT_BORDER_NW: case VGP_HIT_BORDER_SE:
                server->compositor.cursor.shape = VGP_CURSOR_RESIZE_NW; break;
            case VGP_HIT_TITLEBAR:
                server->compositor.cursor.shape = VGP_CURSOR_MOVE; break;
            case VGP_HIT_CONTENT:
                server->compositor.cursor.shape = VGP_CURSOR_TEXT; break;
            default:
                server->compositor.cursor.shape = VGP_CURSOR_ARROW; break;
            }
        } else if (!server->compositor.grab.active) {
            server->compositor.cursor.shape = VGP_CURSOR_ARROW;
        }
    }

    /* Update which output the cursor is on */
    server->compositor.active_output =
        vgp_compositor_output_at_cursor(&server->compositor);

    /* Render all outputs, each showing its own workspace */
    for (int i = 0; i < server->drm.output_count; i++) {
        vgp_renderer_render_output(&server->renderer, &server->drm,
                                    &server->drm.outputs[i], i,
                                    &server->compositor,
                                    &server->config.theme,
                                    &server->notify,
                                    &server->animations,
                                    &server->lockscreen,
                                    server->desktop_menu.visible ?
                                        &server->desktop_menu : &server->window_menu,
                                    &server->calendar,
                                    &server->config.panel);
    }
}
