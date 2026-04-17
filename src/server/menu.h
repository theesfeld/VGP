/* SPDX-License-Identifier: MIT */
#ifndef VGP_MENU_H
#define VGP_MENU_H

#include "render_backend.h"
#include <stdbool.h>

#define VGP_MENU_MAX_ITEMS 16

typedef void (*vgp_menu_action_t)(void *server, int item_idx);

typedef struct vgp_menu_item {
    char    label[64];
    bool    separator;
    vgp_menu_action_t action;
} vgp_menu_item_t;

typedef struct vgp_menu {
    vgp_menu_item_t items[VGP_MENU_MAX_ITEMS];
    int             count;
    float           x, y;
    float           width;
    int             hover_idx;
    bool            visible;
} vgp_menu_t;

void vgp_menu_init(vgp_menu_t *menu);
void vgp_menu_show(vgp_menu_t *menu, float x, float y);
void vgp_menu_hide(vgp_menu_t *menu);
void vgp_menu_add(vgp_menu_t *menu, const char *label, vgp_menu_action_t action);
void vgp_menu_add_separator(vgp_menu_t *menu);

/* Returns true if menu handled the click (consumed) */
bool vgp_menu_click(vgp_menu_t *menu, float mx, float my, void *server);
void vgp_menu_hover(vgp_menu_t *menu, float mx, float my);

void vgp_menu_render(vgp_menu_t *menu, vgp_render_backend_t *b, void *ctx,
                      float font_size);

#endif /* VGP_MENU_H */