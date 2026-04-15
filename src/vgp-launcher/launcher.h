#ifndef VGP_LAUNCHER_H
#define VGP_LAUNCHER_H

#include "vgp/vgp.h"
#include <plutovg.h>
#include <stdbool.h>
#include <stdint.h>

#define LAUNCHER_MAX_APPS        2048
#define LAUNCHER_MAX_NAME        128
#define LAUNCHER_MAX_EXEC        512
#define LAUNCHER_INPUT_MAX       256
#define LAUNCHER_VISIBLE_ITEMS   15
#define LAUNCHER_WIDTH           600
#define LAUNCHER_HEIGHT          460

typedef struct launcher_app {
    char name[LAUNCHER_MAX_NAME];
    char exec[LAUNCHER_MAX_EXEC];
    char name_lower[LAUNCHER_MAX_NAME];
    bool terminal;
} launcher_app_t;

typedef struct launcher_app_list {
    launcher_app_t apps[LAUNCHER_MAX_APPS];
    int count;
} launcher_app_list_t;

typedef struct launcher_filtered {
    int app_index;
    int score;
} launcher_filtered_t;

typedef struct launcher {
    vgp_connection_t    *conn;
    uint32_t             window_id;

    plutovg_surface_t   *surface;
    plutovg_canvas_t    *canvas;
    plutovg_font_face_t *font_face;
    float                font_size;

    launcher_app_list_t  app_list;

    char                 input_buf[LAUNCHER_INPUT_MAX];
    int                  input_len;

    launcher_filtered_t  filtered[LAUNCHER_MAX_APPS];
    int                  filtered_count;
    int                  selected_index;
    int                  scroll_offset;

    float                item_height;
    float                padding;

    bool running;
    bool dirty;
} launcher_t;

int  launcher_init(launcher_t *l);
void launcher_destroy(launcher_t *l);
void launcher_run(launcher_t *l);

int  launcher_scan_apps(launcher_app_list_t *list);
void launcher_filter(launcher_t *l);
void launcher_render(launcher_t *l);
void launcher_launch_selected(launcher_t *l);
int  launcher_fuzzy_score(const char *haystack, const char *needle);

#endif /* VGP_LAUNCHER_H */
