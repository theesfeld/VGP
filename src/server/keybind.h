#ifndef VGP_KEYBIND_H
#define VGP_KEYBIND_H

#include "keyboard.h"
#include "config.h"
#include <xkbcommon/xkbcommon.h>
#include <stdbool.h>
#include <stdint.h>

#define VGP_MAX_KEYBINDS 128
#define VGP_KEYBIND_CMD_MAX 256

typedef enum {
    VGP_ACTION_NONE = 0,
    VGP_ACTION_SPAWN_TERMINAL,
    VGP_ACTION_SPAWN_LAUNCHER,
    VGP_ACTION_CLOSE_WINDOW,
    VGP_ACTION_MAXIMIZE_WINDOW,
    VGP_ACTION_MINIMIZE_WINDOW,
    VGP_ACTION_FULLSCREEN,
    VGP_ACTION_FOCUS_NEXT,
    VGP_ACTION_FOCUS_PREV,
    VGP_ACTION_WORKSPACE_1,
    VGP_ACTION_WORKSPACE_2,
    VGP_ACTION_WORKSPACE_3,
    VGP_ACTION_WORKSPACE_4,
    VGP_ACTION_WORKSPACE_5,
    VGP_ACTION_WORKSPACE_6,
    VGP_ACTION_WORKSPACE_7,
    VGP_ACTION_WORKSPACE_8,
    VGP_ACTION_WORKSPACE_9,
    VGP_ACTION_QUIT,
    VGP_ACTION_SCREENSHOT,
    VGP_ACTION_EXPOSE,
    VGP_ACTION_LOCK,
    VGP_ACTION_TOGGLE_FLOAT,
    VGP_ACTION_SNAP_LEFT,
    VGP_ACTION_SNAP_RIGHT,
    VGP_ACTION_SNAP_TOP,
    VGP_ACTION_SNAP_BOTTOM,
    VGP_ACTION_MOVE_TO_WORKSPACE_1,
    VGP_ACTION_MOVE_TO_WORKSPACE_2,
    VGP_ACTION_MOVE_TO_WORKSPACE_3,
    VGP_ACTION_MOVE_TO_WORKSPACE_4,
    VGP_ACTION_MOVE_TO_WORKSPACE_5,
    VGP_ACTION_MOVE_TO_WORKSPACE_6,
    VGP_ACTION_MOVE_TO_WORKSPACE_7,
    VGP_ACTION_MOVE_TO_WORKSPACE_8,
    VGP_ACTION_MOVE_TO_WORKSPACE_9,
    VGP_ACTION_EXEC,
} vgp_action_type_t;

typedef struct vgp_keybind {
    uint32_t          modifiers;
    xkb_keysym_t      keysym;
    vgp_action_type_t action;
    char              cmd[VGP_KEYBIND_CMD_MAX];
} vgp_keybind_t;

typedef struct vgp_keybind_manager {
    vgp_keybind_t binds[VGP_MAX_KEYBINDS];
    int           count;
} vgp_keybind_manager_t;

struct vgp_server;

int  vgp_keybind_init(vgp_keybind_manager_t *mgr,
                       const vgp_config_t *config);

const vgp_keybind_t *vgp_keybind_match(const vgp_keybind_manager_t *mgr,
                                         const vgp_key_event_t *event);

void vgp_keybind_execute(struct vgp_server *server,
                           const vgp_keybind_t *bind);

#endif /* VGP_KEYBIND_H */
