/* SPDX-License-Identifier: MIT */
#include "compositor.h"
#include "tiling.h"
#include "vgp/log.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define TAG "compositor"

int vgp_compositor_init(vgp_compositor_t *comp)
{
    memset(comp, 0, sizeof(*comp));
    vgp_cursor_init(&comp->cursor);
    VGP_LOG_INFO(TAG, "compositor initialized");
    return 0;
}

void vgp_compositor_destroy(vgp_compositor_t *comp)
{
    for (int i = 0; i < VGP_MAX_WINDOWS; i++) {
        if (comp->windows[i].used) {
            free(comp->windows[i].client_pixels);
            comp->windows[i].client_pixels = NULL;
        }
    }
    comp->window_count = 0;
    comp->focused = NULL;
}

void vgp_compositor_set_outputs(vgp_compositor_t *comp,
                                 int count,
                                 const uint32_t *widths,
                                 const uint32_t *heights)
{
    comp->output_count = count > VGP_MAX_OUTPUTS ? VGP_MAX_OUTPUTS : count;

    /* Lay out outputs side by side (left to right) */
    int32_t x_offset = 0;
    for (int i = 0; i < comp->output_count; i++) {
        comp->outputs[i].x = x_offset;
        comp->outputs[i].y = 0;
        comp->outputs[i].width = widths[i];
        comp->outputs[i].height = heights[i];
        comp->outputs[i].workspace = i; /* workspace 0 on monitor 0, etc. */
        comp->outputs[i].active = true;
        x_offset += (int32_t)widths[i];

        VGP_LOG_INFO(TAG, "output %d: workspace %d at %d,0 (%ux%u)",
                     i, i, comp->outputs[i].x, widths[i], heights[i]);
    }

    /* Center cursor on the first output */
    if (comp->output_count > 0) {
        vgp_cursor_set_position(&comp->cursor,
                                 (float)widths[0] / 2.0f,
                                 (float)heights[0] / 2.0f);
    }
}

int vgp_compositor_output_at_cursor(vgp_compositor_t *comp)
{
    float cx = comp->cursor.x, cy = comp->cursor.y;

    for (int i = 0; i < comp->output_count; i++) {
        vgp_output_info_t *out = &comp->outputs[i];
        if (cx >= (float)out->x && cx < (float)(out->x + (int32_t)out->width) &&
            cy >= (float)out->y && cy < (float)(out->y + (int32_t)out->height))
            return i;
    }
    return 0;
}

vgp_window_t *vgp_compositor_create_window(vgp_compositor_t *comp,
                                            int client_fd,
                                            int32_t x, int32_t y,
                                            uint32_t w, uint32_t h,
                                            const char *title,
                                            const vgp_theme_t *theme)
{
    vgp_window_t *win = NULL;
    for (int i = 0; i < VGP_MAX_WINDOWS; i++) {
        if (!comp->windows[i].used) {
            win = &comp->windows[i];
            break;
        }
    }

    if (!win) {
        VGP_LOG_ERROR(TAG, "max windows reached (%d)", VGP_MAX_WINDOWS);
        return NULL;
    }

    memset(win, 0, sizeof(*win));
    win->id = (uint32_t)(win - comp->windows) + 1;
    win->used = true;
    win->state = VGP_WIN_NORMAL;
    win->visible = true;
    win->decorated = true;
    win->client_fd = client_fd;

    /* Assign to the workspace of the output under the cursor */
    int out_idx = vgp_compositor_output_at_cursor(comp);
    win->workspace = comp->outputs[out_idx].workspace;

    /* Position relative to the output's coordinate space.
     * Account for panel position (top or bottom). */
    vgp_output_info_t *out = &comp->outputs[out_idx];
    float bar_h = theme->statusbar_height;
    int32_t usable_y = comp->panel_top ? (int32_t)bar_h : 0;
    int32_t usable_h = (int32_t)out->height - (int32_t)bar_h;
    if (x < 0) x = out->x + (int32_t)(out->width / 4);
    if (y < 0) y = usable_y + usable_h / 4;

    win->frame_rect = vgp_window_frame_rect(x, y, w, h, theme);
    win->content_rect = vgp_window_content_rect(&win->frame_rect, theme);

    if (title)
        snprintf(win->title, sizeof(win->title), "%s", title);

    comp->z_order[comp->window_count] = win;
    win->z_index = comp->window_count;
    comp->window_count++;

    vgp_compositor_focus_window(comp, win);

    VGP_LOG_INFO(TAG, "created window %u on workspace %d: %ux%u+%d+%d \"%s\"",
                 win->id, win->workspace, w, h, x, y, win->title);

    return win;
}

void vgp_compositor_destroy_window(vgp_compositor_t *comp,
                                    vgp_window_t *win)
{
    if (!win || !win->used)
        return;

    VGP_LOG_INFO(TAG, "destroying window %u \"%s\"", win->id, win->title);

    int idx = -1;
    for (int i = 0; i < comp->window_count; i++) {
        if (comp->z_order[i] == win) { idx = i; break; }
    }
    if (idx >= 0) {
        memmove(&comp->z_order[idx], &comp->z_order[idx + 1],
                (size_t)(comp->window_count - idx - 1) * sizeof(vgp_window_t *));
        comp->window_count--;
    }

    if (comp->focused == win) {
        comp->focused = NULL;
        for (int i = comp->window_count - 1; i >= 0; i--) {
            if (comp->z_order[i]->visible &&
                comp->z_order[i]->workspace == win->workspace) {
                vgp_compositor_focus_window(comp, comp->z_order[i]);
                break;
            }
        }
    }

    if (comp->grab.target == win) {
        comp->grab.active = false;
        comp->grab.target = NULL;
    }

    free(win->client_pixels);
    win->client_pixels = NULL;
    free(win->cellgrid);
    win->cellgrid = NULL;
    win->has_cellgrid = false;

    win->used = false;
}

void vgp_compositor_focus_window(vgp_compositor_t *comp,
                                  vgp_window_t *win)
{
    if (comp->focused == win) return;
    if (comp->focused) comp->focused->focused = false;
    comp->focused = win;
    if (win) {
        win->focused = true;
        vgp_compositor_raise_window(comp, win);
    }
}

void vgp_compositor_raise_window(vgp_compositor_t *comp,
                                  vgp_window_t *win)
{
    int idx = -1;
    for (int i = 0; i < comp->window_count; i++) {
        if (comp->z_order[i] == win) { idx = i; break; }
    }
    if (idx < 0 || idx == comp->window_count - 1) return;

    memmove(&comp->z_order[idx], &comp->z_order[idx + 1],
            (size_t)(comp->window_count - idx - 1) * sizeof(vgp_window_t *));
    comp->z_order[comp->window_count - 1] = win;
    for (int i = idx; i < comp->window_count; i++)
        comp->z_order[i]->z_index = i;
}

vgp_window_t *vgp_compositor_window_at(vgp_compositor_t *comp,
                                        int32_t x, int32_t y)
{
    /* Determine which workspace the click is on */
    int ws = -1;
    for (int i = 0; i < comp->output_count; i++) {
        vgp_output_info_t *out = &comp->outputs[i];
        if (x >= out->x && x < out->x + (int32_t)out->width &&
            y >= out->y && y < out->y + (int32_t)out->height) {
            ws = out->workspace;
            break;
        }
    }

    for (int i = comp->window_count - 1; i >= 0; i--) {
        vgp_window_t *win = comp->z_order[i];
        if (!win->visible || win->state == VGP_WIN_MINIMIZED) continue;
        if (win->workspace != ws) continue;
        if (vgp_rect_contains_point(&win->frame_rect, x, y))
            return win;
    }
    return NULL;
}

void vgp_compositor_minimize_window(vgp_compositor_t *comp,
                                     vgp_window_t *win)
{
    if (win->state == VGP_WIN_MINIMIZED) return;
    win->state = VGP_WIN_MINIMIZED;
    win->visible = false;
    if (comp->focused == win) {
        comp->focused = NULL;
        for (int i = comp->window_count - 1; i >= 0; i--) {
            if (comp->z_order[i]->visible &&
                comp->z_order[i]->workspace == win->workspace &&
                comp->z_order[i] != win) {
                vgp_compositor_focus_window(comp, comp->z_order[i]);
                break;
            }
        }
    }
}

void vgp_compositor_maximize_window(vgp_compositor_t *comp,
                                     vgp_window_t *win,
                                     uint32_t output_w, uint32_t output_h,
                                     const vgp_theme_t *theme)
{
    if (win->state == VGP_WIN_MAXIMIZED) {
        vgp_compositor_restore_window(comp, win);
        return;
    }

    win->saved_rect = win->frame_rect;
    win->state = VGP_WIN_MAXIMIZED;

    /* Find the output this window's workspace is displayed on */
    int32_t ox = 0;
    for (int i = 0; i < comp->output_count; i++) {
        if (comp->outputs[i].workspace == win->workspace) {
            ox = comp->outputs[i].x;
            output_w = comp->outputs[i].width;
            output_h = comp->outputs[i].height;
            break;
        }
    }

    float bar_h = theme->statusbar_height;
    int32_t wy = comp->panel_top ? (int32_t)bar_h : 0;
    win->frame_rect = (vgp_rect_t){
        ox, wy, (int32_t)output_w, (int32_t)output_h - (int32_t)bar_h,
    };
    win->content_rect = vgp_window_content_rect(&win->frame_rect, theme);
}

void vgp_compositor_restore_window(vgp_compositor_t *comp,
                                    vgp_window_t *win)
{
    (void)comp;
    if (win->state == VGP_WIN_MINIMIZED)
        win->visible = true;
    win->state = VGP_WIN_NORMAL;
    win->frame_rect = win->saved_rect;
}

void vgp_compositor_move_window(vgp_compositor_t *comp,
                                 vgp_window_t *win,
                                 int32_t x, int32_t y,
                                 const vgp_theme_t *theme)
{
    win->frame_rect.x = x;
    win->frame_rect.y = y;
    win->content_rect = vgp_window_content_rect(&win->frame_rect, theme);

    /* Re-home the window to whichever output contains its center.
     * Each output drives one workspace -- without this the window would
     * keep rendering on its origin output while its coordinates sit on
     * a neighbour, making it "vanish" mid-drag. */
    int32_t cx = win->frame_rect.x + win->frame_rect.w / 2;
    int32_t cy = win->frame_rect.y + win->frame_rect.h / 2;
    for (int i = 0; i < comp->output_count; i++) {
        vgp_output_info_t *out = &comp->outputs[i];
        if (cx >= out->x && cx < out->x + (int32_t)out->width &&
            cy >= out->y && cy < out->y + (int32_t)out->height) {
            if (win->workspace != out->workspace)
                win->workspace = out->workspace;
            break;
        }
    }
}

void vgp_compositor_resize_window(vgp_compositor_t *comp,
                                   vgp_window_t *win,
                                   uint32_t w, uint32_t h,
                                   const vgp_theme_t *theme)
{
    (void)comp;
    if (w < 100) w = 100;
    if (h < 60) h = 60;
    win->frame_rect.w = (int32_t)w;
    win->frame_rect.h = (int32_t)h;
    win->content_rect = vgp_window_content_rect(&win->frame_rect, theme);
}

void vgp_compositor_focus_cycle(vgp_compositor_t *comp, int direction)
{
    if (comp->window_count == 0) return;

    /* Get the workspace of the active output */
    int ws = comp->outputs[comp->active_output].workspace;

    int current_idx = -1;
    if (comp->focused) {
        for (int i = 0; i < comp->window_count; i++) {
            if (comp->z_order[i] == comp->focused) {
                current_idx = i;
                break;
            }
        }
    }

    for (int step = 1; step <= comp->window_count; step++) {
        int idx = (current_idx + direction * step + comp->window_count * 2)
                  % comp->window_count;
        vgp_window_t *win = comp->z_order[idx];
        if (win->visible && win->state != VGP_WIN_MINIMIZED &&
            win->workspace == ws) {
            vgp_compositor_focus_window(comp, win);
            return;
        }
    }
}

void vgp_compositor_retile(vgp_compositor_t *comp, int workspace,
                            struct vgp_tile_config *tile_config,
                            const vgp_theme_t *theme)
{
    if (!tile_config) return;

    /* Find the output showing this workspace */
    vgp_output_info_t *out = NULL;
    for (int i = 0; i < comp->output_count; i++) {
        if (comp->outputs[i].workspace == workspace) {
            out = &comp->outputs[i];
            break;
        }
    }
    if (!out) return;

    /* Usable area (minus panel, offset for top panel) */
    float bar_h = theme->statusbar_height;
    int32_t area_y = comp->panel_top ? (int32_t)bar_h : 0;
    vgp_rect_t area = {
        out->x, area_y,
        (int32_t)out->width,
        (int32_t)out->height - (int32_t)bar_h,
    };

    /* Collect tileable windows on this workspace */
    vgp_window_t *tile_wins[VGP_TILE_MAX_WINDOWS];
    int tile_count = 0;

    for (int i = 0; i < comp->window_count; i++) {
        vgp_window_t *w = comp->z_order[i];
        if (!w->visible || w->state == VGP_WIN_MINIMIZED)
            continue;
        if (w->workspace != workspace)
            continue;
        if (!w->decorated) /* override windows don't tile */
            continue;
        if (w->floating_override) /* explicitly floating */
            continue;
        if (tile_count >= VGP_TILE_MAX_WINDOWS)
            break;
        tile_wins[tile_count++] = w;
    }

    if (tile_count == 0) return;

    /* Calculate tiled positions */
    vgp_rect_t rects[VGP_TILE_MAX_WINDOWS];
    int n = vgp_tile_calculate(tile_config, tile_count, area, rects);

    /* Apply positions to windows */
    for (int i = 0; i < n; i++) {
        tile_wins[i]->frame_rect = rects[i];
        tile_wins[i]->content_rect = vgp_window_content_rect(&rects[i], theme);
        tile_wins[i]->state = VGP_WIN_NORMAL;
    }
}