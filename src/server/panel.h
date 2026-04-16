#ifndef VGP_PANEL_H
#define VGP_PANEL_H

#include "config.h"
#include "compositor.h"
#include "render_backend.h"
#include "theme.h"
#include "calendar.h"

/* Panel geometry for a given output */
typedef struct vgp_panel_geom {
    float x, y, w, h;     /* panel rect in output-local coords */
    bool  top;             /* true = top, false = bottom */
} vgp_panel_geom_t;

/* Compute panel geometry for an output */
vgp_panel_geom_t vgp_panel_geometry(const vgp_config_panel_t *cfg,
                                      const vgp_theme_t *theme,
                                      uint32_t output_w, uint32_t output_h);

/* Render the panel for one output */
void vgp_panel_render(vgp_render_backend_t *b, void *ctx,
                       const vgp_theme_t *theme,
                       const vgp_config_panel_t *cfg,
                       uint32_t output_w, uint32_t output_h,
                       int workspace,
                       vgp_compositor_t *comp);

/* Hit-test a click in output-local coordinates.
 * Returns true if the click was in the panel and was handled.
 * May modify compositor state (workspace switch, window focus). */
struct vgp_server;
bool vgp_panel_click(const vgp_config_panel_t *cfg,
                      const vgp_theme_t *theme,
                      float local_x, float local_y,
                      uint32_t output_w, uint32_t output_h,
                      struct vgp_server *server,
                      int output_idx);

/* Returns the usable area for windows (output minus panel) */
float vgp_panel_usable_height(const vgp_config_panel_t *cfg,
                                const vgp_theme_t *theme,
                                uint32_t output_h);

#endif /* VGP_PANEL_H */
