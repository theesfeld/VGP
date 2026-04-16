#include "menu.h"
#include <stdio.h>
#include <string.h>

#define ITEM_HEIGHT 26.0f
#define MENU_PADDING 6.0f

void vgp_menu_init(vgp_menu_t *menu)
{
    memset(menu, 0, sizeof(*menu));
    menu->hover_idx = -1;
    menu->width = 200.0f;
}

void vgp_menu_show(vgp_menu_t *menu, float x, float y)
{
    /* Offset menu slightly so cursor doesn't overlap items */
    menu->x = x + 2;
    menu->y = y + 2;
    menu->visible = true;
    menu->hover_idx = -1;
}

void vgp_menu_hide(vgp_menu_t *menu)
{
    menu->visible = false;
    menu->hover_idx = -1;
}

void vgp_menu_add(vgp_menu_t *menu, const char *label, vgp_menu_action_t action)
{
    if (menu->count >= VGP_MENU_MAX_ITEMS) return;
    vgp_menu_item_t *item = &menu->items[menu->count++];
    snprintf(item->label, sizeof(item->label), "%s", label);
    item->separator = false;
    item->action = action;
}

void vgp_menu_add_separator(vgp_menu_t *menu)
{
    if (menu->count >= VGP_MENU_MAX_ITEMS) return;
    vgp_menu_item_t *item = &menu->items[menu->count++];
    item->separator = true;
    item->action = NULL;
}

bool vgp_menu_click(vgp_menu_t *menu, float mx, float my, void *server)
{
    if (!menu->visible) return false;

    float total_h = (float)menu->count * ITEM_HEIGHT + MENU_PADDING * 2;
    if (mx < menu->x || mx > menu->x + menu->width ||
        my < menu->y || my > menu->y + total_h) {
        vgp_menu_hide(menu);
        return true; /* consumed -- clicking outside closes menu */
    }

    int idx = (int)((my - menu->y - MENU_PADDING) / ITEM_HEIGHT);
    if (idx >= 0 && idx < menu->count && !menu->items[idx].separator) {
        if (menu->items[idx].action)
            menu->items[idx].action(server, idx);
        vgp_menu_hide(menu);
    }
    return true;
}

void vgp_menu_hover(vgp_menu_t *menu, float mx, float my)
{
    if (!menu->visible) return;
    float total_h = (float)menu->count * ITEM_HEIGHT + MENU_PADDING * 2;
    if (mx < menu->x || mx > menu->x + menu->width ||
        my < menu->y || my > menu->y + total_h) {
        menu->hover_idx = -1;
        return;
    }
    menu->hover_idx = (int)((my - menu->y - MENU_PADDING) / ITEM_HEIGHT);
}

void vgp_menu_render(vgp_menu_t *menu, vgp_render_backend_t *b, void *ctx,
                      float font_size)
{
    if (!menu->visible) return;

    float total_h = (float)menu->count * ITEM_HEIGHT + MENU_PADDING * 2;

    /* Shadow */
    b->ops->draw_rounded_rect(b, ctx,
        menu->x + 4, menu->y + 4, menu->width, total_h, 6,
        0, 0, 0, 0.3f);

    /* Background */
    b->ops->draw_rounded_rect(b, ctx,
        menu->x, menu->y, menu->width, total_h, 6,
        0.12f, 0.12f, 0.18f, 0.95f);

    /* Border */
    b->ops->draw_rounded_rect(b, ctx,
        menu->x, menu->y, menu->width, total_h, 6,
        0.3f, 0.3f, 0.4f, 0.3f);

    for (int i = 0; i < menu->count; i++) {
        float iy = menu->y + MENU_PADDING + (float)i * ITEM_HEIGHT;

        if (menu->items[i].separator) {
            b->ops->draw_line(b, ctx,
                menu->x + 8, iy + ITEM_HEIGHT / 2,
                menu->x + menu->width - 8, iy + ITEM_HEIGHT / 2,
                1.0f, 0.3f, 0.3f, 0.4f, 0.4f);
            continue;
        }

        /* Hover highlight */
        if (i == menu->hover_idx) {
            b->ops->draw_rounded_rect(b, ctx,
                menu->x + 4, iy + 2, menu->width - 8, ITEM_HEIGHT - 4, 4,
                0.32f, 0.53f, 0.88f, 0.3f);
        }

        /* Label */
        float text_y = iy + ITEM_HEIGHT / 2 + font_size / 3;
        float alpha = (i == menu->hover_idx) ? 1.0f : 0.8f;
        b->ops->draw_text(b, ctx, menu->items[i].label, -1,
                           menu->x + 16, text_y, font_size,
                           0.85f, 0.85f, 0.85f, alpha);
    }
}
