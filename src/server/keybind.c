#include "keybind.h"
#include "server.h"
#include "lockscreen.h"
#include "tiling.h"
#include "spawn.h"
#include "vgp/log.h"
#include "vgp/protocol.h"

#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <time.h>

#ifdef VGP_HAS_GPU_BACKEND
#include <GLES3/gl3.h>
#endif

#define TAG "keybind"

/* Parse "Super+Shift+Return" into modifiers + keysym */
static int parse_key_string(const char *str, uint32_t *out_mods,
                             xkb_keysym_t *out_keysym)
{
    uint32_t mods = 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s", str);

    /* Tokenize on '+' -- last token is the key, rest are modifiers */
    char *tokens[8];
    int token_count = 0;

    char *saveptr;
    char *tok = strtok_r(buf, "+", &saveptr);
    while (tok && token_count < 8) {
        /* Trim whitespace */
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        tokens[token_count++] = tok;
        tok = strtok_r(NULL, "+", &saveptr);
    }

    if (token_count == 0) return -1;

    /* All tokens except the last are modifiers */
    for (int i = 0; i < token_count - 1; i++) {
        if (strcasecmp(tokens[i], "Super") == 0 ||
            strcasecmp(tokens[i], "Mod4") == 0)
            mods |= VGP_MOD_SUPER;
        else if (strcasecmp(tokens[i], "Ctrl") == 0 ||
                 strcasecmp(tokens[i], "Control") == 0)
            mods |= VGP_MOD_CTRL;
        else if (strcasecmp(tokens[i], "Alt") == 0 ||
                 strcasecmp(tokens[i], "Mod1") == 0)
            mods |= VGP_MOD_ALT;
        else if (strcasecmp(tokens[i], "Shift") == 0)
            mods |= VGP_MOD_SHIFT;
        else {
            VGP_LOG_WARN(TAG, "unknown modifier: %s", tokens[i]);
            return -1;
        }
    }

    /* Last token is the key */
    xkb_keysym_t sym = xkb_keysym_from_name(tokens[token_count - 1],
                                              XKB_KEYSYM_CASE_INSENSITIVE);
    if (sym == XKB_KEY_NoSymbol) {
        VGP_LOG_WARN(TAG, "unknown key: %s", tokens[token_count - 1]);
        return -1;
    }

    *out_mods = mods;
    *out_keysym = sym;
    return 0;
}

static vgp_action_type_t parse_action_string(const char *str, char *cmd_out)
{
    cmd_out[0] = '\0';

    struct { const char *name; vgp_action_type_t action; } map[] = {
        { "spawn_terminal",    VGP_ACTION_SPAWN_TERMINAL },
        { "spawn_launcher",    VGP_ACTION_SPAWN_LAUNCHER },
        { "close_window",      VGP_ACTION_CLOSE_WINDOW },
        { "maximize_window",   VGP_ACTION_MAXIMIZE_WINDOW },
        { "minimize_window",   VGP_ACTION_MINIMIZE_WINDOW },
        { "fullscreen",        VGP_ACTION_FULLSCREEN },
        { "focus_next",        VGP_ACTION_FOCUS_NEXT },
        { "focus_prev",        VGP_ACTION_FOCUS_PREV },
        { "quit",              VGP_ACTION_QUIT },
        { "workspace_1",       VGP_ACTION_WORKSPACE_1 },
        { "workspace_2",       VGP_ACTION_WORKSPACE_2 },
        { "workspace_3",       VGP_ACTION_WORKSPACE_3 },
        { "workspace_4",       VGP_ACTION_WORKSPACE_4 },
        { "workspace_5",       VGP_ACTION_WORKSPACE_5 },
        { "workspace_6",       VGP_ACTION_WORKSPACE_6 },
        { "workspace_7",       VGP_ACTION_WORKSPACE_7 },
        { "workspace_8",       VGP_ACTION_WORKSPACE_8 },
        { "workspace_9",       VGP_ACTION_WORKSPACE_9 },
        { "screenshot",        VGP_ACTION_SCREENSHOT },
        { "expose",            VGP_ACTION_EXPOSE },
        { "lock",              VGP_ACTION_LOCK },
        { "toggle_float",     VGP_ACTION_TOGGLE_FLOAT },
        { "toggle_dark_light", VGP_ACTION_TOGGLE_DARK_LIGHT },
        { "snap_left",         VGP_ACTION_SNAP_LEFT },
        { "snap_right",        VGP_ACTION_SNAP_RIGHT },
        { "snap_top",          VGP_ACTION_SNAP_TOP },
        { "snap_bottom",       VGP_ACTION_SNAP_BOTTOM },
        { "move_to_workspace_1", VGP_ACTION_MOVE_TO_WORKSPACE_1 },
        { "move_to_workspace_2", VGP_ACTION_MOVE_TO_WORKSPACE_2 },
        { "move_to_workspace_3", VGP_ACTION_MOVE_TO_WORKSPACE_3 },
        { "move_to_workspace_4", VGP_ACTION_MOVE_TO_WORKSPACE_4 },
        { "move_to_workspace_5", VGP_ACTION_MOVE_TO_WORKSPACE_5 },
        { "move_to_workspace_6", VGP_ACTION_MOVE_TO_WORKSPACE_6 },
        { "move_to_workspace_7", VGP_ACTION_MOVE_TO_WORKSPACE_7 },
        { "move_to_workspace_8", VGP_ACTION_MOVE_TO_WORKSPACE_8 },
        { "move_to_workspace_9", VGP_ACTION_MOVE_TO_WORKSPACE_9 },
        { NULL, VGP_ACTION_NONE },
    };

    for (int i = 0; map[i].name; i++) {
        if (strcasecmp(str, map[i].name) == 0)
            return map[i].action;
    }

    /* Check for exec: prefix */
    if (strncasecmp(str, "exec:", 5) == 0) {
        snprintf(cmd_out, VGP_KEYBIND_CMD_MAX, "%s", str + 5);
        return VGP_ACTION_EXEC;
    }

    VGP_LOG_WARN(TAG, "unknown action: %s", str);
    return VGP_ACTION_NONE;
}

int vgp_keybind_init(vgp_keybind_manager_t *mgr, const vgp_config_t *config)
{
    memset(mgr, 0, sizeof(*mgr));

    for (int i = 0; i < config->keybind_count; i++) {
        const vgp_keybind_entry_t *entry = &config->keybind_entries[i];

        uint32_t mods;
        xkb_keysym_t keysym;
        if (parse_key_string(entry->key_str, &mods, &keysym) < 0) {
            VGP_LOG_WARN(TAG, "failed to parse keybind: %s", entry->key_str);
            continue;
        }

        char cmd[VGP_KEYBIND_CMD_MAX];
        vgp_action_type_t action = parse_action_string(entry->action_str, cmd);
        if (action == VGP_ACTION_NONE)
            continue;

        if (mgr->count >= VGP_MAX_KEYBINDS) {
            VGP_LOG_WARN(TAG, "max keybinds reached");
            break;
        }

        vgp_keybind_t *bind = &mgr->binds[mgr->count++];
        bind->modifiers = mods;
        bind->keysym = keysym;
        bind->action = action;
        memcpy(bind->cmd, cmd, sizeof(bind->cmd));

        char keysym_name[64];
        xkb_keysym_get_name(keysym, keysym_name, sizeof(keysym_name));
        VGP_LOG_DEBUG(TAG, "keybind: %s -> %s (mods=0x%x sym=%s)",
                      entry->key_str, entry->action_str, mods, keysym_name);
    }

    VGP_LOG_INFO(TAG, "loaded %d keybinds", mgr->count);
    return 0;
}

const vgp_keybind_t *vgp_keybind_match(const vgp_keybind_manager_t *mgr,
                                         const vgp_key_event_t *event)
{
    if (!event->pressed)
        return NULL;

    /* Mask out Caps Lock and Num Lock */
    uint32_t mods = event->modifiers & (VGP_MOD_SHIFT | VGP_MOD_CTRL |
                                         VGP_MOD_ALT | VGP_MOD_SUPER);

    for (int i = 0; i < mgr->count; i++) {
        if (mgr->binds[i].keysym == event->keysym &&
            mgr->binds[i].modifiers == mods)
            return &mgr->binds[i];
    }
    return NULL;
}

void vgp_keybind_execute(struct vgp_server *server, const vgp_keybind_t *bind)
{
    switch (bind->action) {
    case VGP_ACTION_SPAWN_TERMINAL:
        vgp_spawn(server, server->config.general.terminal_cmd);
        break;

    case VGP_ACTION_SPAWN_LAUNCHER:
        vgp_spawn(server, server->config.general.launcher_cmd);
        break;

    case VGP_ACTION_CLOSE_WINDOW: {
        vgp_window_t *focused = server->compositor.focused;
        if (focused) {
            /* Notify client, then force-destroy */
            if (focused->client_fd >= 0) {
                vgp_msg_header_t msg = {
                    .magic = VGP_PROTOCOL_MAGIC,
                    .type = VGP_MSG_WINDOW_CLOSE,
                    .length = sizeof(vgp_msg_header_t),
                    .window_id = focused->id,
                };
                vgp_ipc_client_t *client =
                    vgp_ipc_find_client(&server->ipc, focused->client_fd);
                if (client)
                    vgp_ipc_send(client, &msg, sizeof(msg));
            }
            vgp_compositor_destroy_window(&server->compositor, focused);
            vgp_renderer_schedule_frame(&server->renderer);
        }
        break;
    }

    case VGP_ACTION_MAXIMIZE_WINDOW: {
        vgp_window_t *focused = server->compositor.focused;
        if (focused && server->drm.output_count > 0) {
            vgp_compositor_maximize_window(&server->compositor, focused,
                server->drm.outputs[0].width, server->drm.outputs[0].height,
                &server->config.theme);
            vgp_renderer_schedule_frame(&server->renderer);
        }
        break;
    }

    case VGP_ACTION_MINIMIZE_WINDOW: {
        vgp_window_t *focused = server->compositor.focused;
        if (focused) {
            vgp_compositor_minimize_window(&server->compositor, focused);
            vgp_renderer_schedule_frame(&server->renderer);
        }
        break;
    }

    case VGP_ACTION_FOCUS_NEXT:
        vgp_compositor_focus_cycle(&server->compositor, +1);
        vgp_renderer_schedule_frame(&server->renderer);
        break;

    case VGP_ACTION_FOCUS_PREV:
        vgp_compositor_focus_cycle(&server->compositor, -1);
        vgp_renderer_schedule_frame(&server->renderer);
        break;

    case VGP_ACTION_QUIT:
        VGP_LOG_INFO(TAG, "quit keybind triggered");
        vgp_event_loop_stop(&server->loop);
        break;

    case VGP_ACTION_SCREENSHOT: {
        /* Save screenshot of active output as PPM */
        int out_idx = server->compositor.active_output;
        if (out_idx >= 0 && out_idx < server->drm.output_count) {
            vgp_drm_output_t *out = &server->drm.outputs[out_idx];
            uint32_t w = out->width, h = out->height;

            /* Read pixels from the front buffer (CPU backend) or GL (GPU) */
            uint8_t *pixels = NULL;
#ifdef VGP_HAS_GPU_BACKEND
            if (server->renderer.backend->type == VGP_BACKEND_GPU) {
                pixels = malloc(w * h * 4);
                if (pixels) {
                    glReadPixels(0, 0, (int)w, (int)h, GL_RGBA,
                                  GL_UNSIGNED_BYTE, pixels);
                }
            }
#endif
            if (!pixels && out->fbs[out->front].map) {
                pixels = out->fbs[out->front].map;
            }

            if (pixels) {
                char path[512];
                time_t now = time(NULL);
                struct tm *tm_now = localtime(&now);
                snprintf(path, sizeof(path),
                         "%s/vgp-%04d%02d%02d-%02d%02d%02d.ppm",
                         server->config.general.screenshot_dir,
                         tm_now->tm_year + 1900, tm_now->tm_mon + 1,
                         tm_now->tm_mday, tm_now->tm_hour,
                         tm_now->tm_min, tm_now->tm_sec);

                FILE *f = fopen(path, "wb");
                if (f) {
                    fprintf(f, "P6\n%u %u\n255\n", w, h);
                    /* GL pixels are bottom-up, PPM is top-down */
                    for (int y = (int)h - 1; y >= 0; y--) {
                        for (uint32_t x = 0; x < w; x++) {
                            uint8_t *p = pixels + (y * w + x) * 4;
                            fputc(p[0], f); /* R */
                            fputc(p[1], f); /* G */
                            fputc(p[2], f); /* B */
                        }
                    }
                    fclose(f);
                    VGP_LOG_INFO("screenshot", "saved: %s (%ux%u)", path, w, h);
                }

                if (server->renderer.backend->type == VGP_BACKEND_GPU)
                    free(pixels);
            }
        }
        break;
    }

    case VGP_ACTION_EXEC:
        vgp_spawn(server, bind->cmd);
        break;

    case VGP_ACTION_LOCK:
        vgp_lockscreen_lock(&server->lockscreen);
        vgp_renderer_schedule_frame(&server->renderer);
        break;

    case VGP_ACTION_TOGGLE_DARK_LIGHT: {
        /* Toggle between dark and light theme by reloading config with swapped theme */
        const char *current = server->config.general.theme_name;
        const char *next = strcmp(current, "light") == 0 ? "dark" : "light";
        snprintf(server->config.general.theme_name,
                 sizeof(server->config.general.theme_name), "%s", next);
        /* Reload theme colors */
        vgp_config_load(&server->config, server->config.config_path);
        vgp_renderer_schedule_frame(&server->renderer);
        VGP_LOG_INFO(TAG, "toggled theme: %s -> %s", current, next);
        break;
    }

    case VGP_ACTION_TOGGLE_FLOAT: {
        vgp_window_t *focused = server->compositor.focused;
        if (focused) {
            focused->floating_override = !focused->floating_override;
            /* Re-tile the workspace */
            if (strcmp(server->config.general.wm_mode, "floating") != 0) {
                vgp_tile_config_t tc = {
                    .algorithm = vgp_tile_parse_algorithm(server->config.general.tile_algorithm),
                    .master_ratio = server->config.general.tile_master_ratio,
                    .gap_inner = server->config.general.tile_gap_inner,
                    .gap_outer = server->config.general.tile_gap_outer,
                    .smart_gaps = server->config.general.tile_smart_gaps,
                };
                vgp_compositor_retile(&server->compositor, focused->workspace,
                                       &tc, &server->config.theme);
            }
            vgp_renderer_schedule_frame(&server->renderer);
        }
        break;
    }

    case VGP_ACTION_EXPOSE: {
        server->compositor.expose_active = !server->compositor.expose_active;
        if (server->compositor.expose_active) {
            /* Calculate tiled positions for all visible windows on active workspace */
            int out_idx = server->compositor.active_output;
            vgp_output_info_t *out = &server->compositor.outputs[out_idx];
            int ws = out->workspace;
            float bar_h = server->config.theme.statusbar_height;

            int count = 0;
            for (int i = 0; i < server->compositor.window_count; i++) {
                vgp_window_t *w = server->compositor.z_order[i];
                if (w->visible && w->workspace == ws && w->decorated)
                    count++;
            }

            if (count > 0) {
                /* Grid layout: ceil(sqrt(count)) columns */
                int cols = 1;
                while (cols * cols < count) cols++;
                int rows_needed = (count + cols - 1) / cols;
                float pad = 20.0f;
                float cell_w = ((float)out->width - pad * ((float)cols + 1)) / (float)cols;
                float cell_h = ((float)out->height - bar_h - pad * ((float)rows_needed + 1)) / (float)rows_needed;

                int idx = 0;
                for (int i = 0; i < server->compositor.window_count; i++) {
                    vgp_window_t *w = server->compositor.z_order[i];
                    if (!w->visible || w->workspace != ws || !w->decorated)
                        continue;
                    int col = idx % cols;
                    int row = idx / cols;
                    server->compositor.expose_rects[w->id] = (vgp_rect_t){
                        .x = (int32_t)(pad + (float)col * (cell_w + pad)) + out->x,
                        .y = (int32_t)(pad + (float)row * (cell_h + pad)),
                        .w = (int32_t)cell_w,
                        .h = (int32_t)cell_h,
                    };
                    idx++;
                }
            }
            VGP_LOG_INFO(TAG, "expose mode ON (%d windows)", count);
        } else {
            VGP_LOG_INFO(TAG, "expose mode OFF");
        }
        vgp_renderer_schedule_frame(&server->renderer);
        break;
    }

    case VGP_ACTION_SNAP_LEFT:
    case VGP_ACTION_SNAP_RIGHT:
    case VGP_ACTION_SNAP_TOP:
    case VGP_ACTION_SNAP_BOTTOM: {
        vgp_window_t *focused = server->compositor.focused;
        if (!focused) break;

        /* Find the output this window's workspace is on */
        int32_t ox = 0;
        uint32_t ow = 1920, oh = 1080;
        float bar_h = server->config.theme.statusbar_height;
        for (int i = 0; i < server->compositor.output_count; i++) {
            if (server->compositor.outputs[i].workspace == focused->workspace) {
                ox = server->compositor.outputs[i].x;
                ow = server->compositor.outputs[i].width;
                oh = server->compositor.outputs[i].height;
                break;
            }
        }
        uint32_t usable_h = (uint32_t)((float)oh - bar_h);

        focused->saved_rect = focused->frame_rect;
        focused->state = VGP_WIN_MAXIMIZED; /* so restore works */

        if (bind->action == VGP_ACTION_SNAP_LEFT)
            focused->frame_rect = (vgp_rect_t){ ox, 0, (int32_t)(ow / 2), (int32_t)usable_h };
        else if (bind->action == VGP_ACTION_SNAP_RIGHT)
            focused->frame_rect = (vgp_rect_t){ ox + (int32_t)(ow / 2), 0, (int32_t)(ow / 2), (int32_t)usable_h };
        else if (bind->action == VGP_ACTION_SNAP_TOP)
            focused->frame_rect = (vgp_rect_t){ ox, 0, (int32_t)ow, (int32_t)(usable_h / 2) };
        else
            focused->frame_rect = (vgp_rect_t){ ox, (int32_t)(usable_h / 2), (int32_t)ow, (int32_t)(usable_h / 2) };

        focused->content_rect = vgp_window_content_rect(&focused->frame_rect,
                                                          &server->config.theme);
        vgp_server_send_configure(server, focused);
        vgp_renderer_schedule_frame(&server->renderer);
        break;
    }

    case VGP_ACTION_MOVE_TO_WORKSPACE_1:
    case VGP_ACTION_MOVE_TO_WORKSPACE_2:
    case VGP_ACTION_MOVE_TO_WORKSPACE_3:
    case VGP_ACTION_MOVE_TO_WORKSPACE_4:
    case VGP_ACTION_MOVE_TO_WORKSPACE_5:
    case VGP_ACTION_MOVE_TO_WORKSPACE_6:
    case VGP_ACTION_MOVE_TO_WORKSPACE_7:
    case VGP_ACTION_MOVE_TO_WORKSPACE_8:
    case VGP_ACTION_MOVE_TO_WORKSPACE_9: {
        vgp_window_t *focused = server->compositor.focused;
        if (!focused) break;
        int target_ws = bind->action - VGP_ACTION_MOVE_TO_WORKSPACE_1;
        focused->workspace = target_ws;

        /* Reposition window to the target workspace's output */
        for (int i = 0; i < server->compositor.output_count; i++) {
            if (server->compositor.outputs[i].workspace == target_ws) {
                int32_t new_x = server->compositor.outputs[i].x +
                    (int32_t)(server->compositor.outputs[i].width / 4);
                int32_t new_y = (int32_t)(server->compositor.outputs[i].height / 4);
                vgp_compositor_move_window(&server->compositor, focused,
                                            new_x, new_y, &server->config.theme);
                break;
            }
        }
        vgp_renderer_schedule_frame(&server->renderer);
        break;
    }

    default:
        /* Workspace switch: change which workspace the active output shows */
        if (bind->action >= VGP_ACTION_WORKSPACE_1 &&
            bind->action <= VGP_ACTION_WORKSPACE_9) {
            int target_ws = bind->action - VGP_ACTION_WORKSPACE_1;
            int active_out = server->compositor.active_output;
            if (active_out >= 0 && active_out < server->compositor.output_count) {
                server->compositor.outputs[active_out].workspace = target_ws;
                VGP_LOG_INFO(TAG, "output %d -> workspace %d", active_out, target_ws);
                vgp_renderer_schedule_frame(&server->renderer);
            }
        }
        break;
    }
}
