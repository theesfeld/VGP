/* SPDX-License-Identifier: MIT */
#include "keyboard.h"
#include "vgp/log.h"

#include <string.h>

#define TAG "keyboard"

int vgp_keyboard_init(vgp_keyboard_t *kb)
{
    memset(kb, 0, sizeof(*kb));

    kb->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    if (!kb->xkb_ctx) {
        VGP_LOG_ERROR(TAG, "xkb_context_new failed");
        return -1;
    }

    /* Use default keymap (from environment or system defaults) */
    kb->xkb_keymap = xkb_keymap_new_from_names(kb->xkb_ctx, NULL,
                                                 XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!kb->xkb_keymap) {
        VGP_LOG_ERROR(TAG, "xkb_keymap_new_from_names failed");
        xkb_context_unref(kb->xkb_ctx);
        kb->xkb_ctx = NULL;
        return -1;
    }

    kb->xkb_state = xkb_state_new(kb->xkb_keymap);
    if (!kb->xkb_state) {
        VGP_LOG_ERROR(TAG, "xkb_state_new failed");
        xkb_keymap_unref(kb->xkb_keymap);
        xkb_context_unref(kb->xkb_ctx);
        kb->xkb_keymap = NULL;
        kb->xkb_ctx = NULL;
        return -1;
    }

    VGP_LOG_INFO(TAG, "xkbcommon initialized");
    return 0;
}

void vgp_keyboard_destroy(vgp_keyboard_t *kb)
{
    if (kb->xkb_state) {
        xkb_state_unref(kb->xkb_state);
        kb->xkb_state = NULL;
    }
    if (kb->xkb_keymap) {
        xkb_keymap_unref(kb->xkb_keymap);
        kb->xkb_keymap = NULL;
    }
    if (kb->xkb_ctx) {
        xkb_context_unref(kb->xkb_ctx);
        kb->xkb_ctx = NULL;
    }
}

void vgp_keyboard_process_key(vgp_keyboard_t *kb, uint32_t keycode,
                               bool pressed, vgp_key_event_t *event)
{
    /* libinput keycodes are evdev codes, xkb expects +8 */
    xkb_keycode_t xkb_keycode = keycode + 8;

    memset(event, 0, sizeof(*event));
    event->keycode = keycode;
    event->pressed = pressed;

    /* Get keysym */
    event->keysym = xkb_state_key_get_one_sym(kb->xkb_state, xkb_keycode);

    /* Get UTF-8 representation */
    if (pressed) {
        int len = xkb_state_key_get_utf8(kb->xkb_state, xkb_keycode,
                                          event->utf8, sizeof(event->utf8));
        if (len > 0 && len < (int)sizeof(event->utf8))
            event->utf8_len = (uint32_t)len;
    }

    /* Get modifiers */
    event->modifiers = vgp_keyboard_get_modifiers(kb);

    /* Update state */
    xkb_state_update_key(kb->xkb_state, xkb_keycode,
                          pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
}

uint32_t vgp_keyboard_get_modifiers(vgp_keyboard_t *kb)
{
    uint32_t mods = 0;

    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_SHIFT,
                                      XKB_STATE_MODS_EFFECTIVE))
        mods |= VGP_MOD_SHIFT;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_CTRL,
                                      XKB_STATE_MODS_EFFECTIVE))
        mods |= VGP_MOD_CTRL;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_ALT,
                                      XKB_STATE_MODS_EFFECTIVE))
        mods |= VGP_MOD_ALT;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_LOGO,
                                      XKB_STATE_MODS_EFFECTIVE))
        mods |= VGP_MOD_SUPER;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_CAPS,
                                      XKB_STATE_MODS_EFFECTIVE))
        mods |= VGP_MOD_CAPS;
    if (xkb_state_mod_name_is_active(kb->xkb_state, XKB_MOD_NAME_NUM,
                                      XKB_STATE_MODS_EFFECTIVE))
        mods |= VGP_MOD_NUM;

    return mods;
}