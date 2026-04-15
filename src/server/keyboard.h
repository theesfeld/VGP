#ifndef VGP_KEYBOARD_H
#define VGP_KEYBOARD_H

#include <xkbcommon/xkbcommon.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct vgp_keyboard {
    struct xkb_context *xkb_ctx;
    struct xkb_keymap  *xkb_keymap;
    struct xkb_state   *xkb_state;
} vgp_keyboard_t;

typedef struct vgp_key_event {
    uint32_t keycode;
    uint32_t keysym;
    uint32_t modifiers;
    char     utf8[8];
    uint32_t utf8_len;
    bool     pressed;
} vgp_key_event_t;

/* Modifier flags */
#define VGP_MOD_SHIFT  0x0001
#define VGP_MOD_CTRL   0x0002
#define VGP_MOD_ALT    0x0004
#define VGP_MOD_SUPER  0x0008
#define VGP_MOD_CAPS   0x0010
#define VGP_MOD_NUM    0x0020

int  vgp_keyboard_init(vgp_keyboard_t *kb);
void vgp_keyboard_destroy(vgp_keyboard_t *kb);

/* Process a key event, fill in key_event struct */
void vgp_keyboard_process_key(vgp_keyboard_t *kb, uint32_t keycode,
                               bool pressed, vgp_key_event_t *event);

/* Get current modifier state */
uint32_t vgp_keyboard_get_modifiers(vgp_keyboard_t *kb);

#endif /* VGP_KEYBOARD_H */
