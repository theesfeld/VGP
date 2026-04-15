/* VGP Image Viewer -- display images using pixel surface protocol */

#include "vgp/vgp.h"
#include "vgp/protocol.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#include "stb_image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>

static vgp_connection_t *conn;
static uint32_t window_id;
static bool running = true;
static uint8_t *pixels = NULL;
static int img_w, img_h;

static void on_event(vgp_connection_t *c, const vgp_event_t *ev, void *data)
{
    (void)c; (void)data;
    if (ev->type == VGP_EVENT_CLOSE ||
        (ev->type == VGP_EVENT_KEY_PRESS && ev->key.keysym == 0xFF1B))
        running = false;
}

static int load_image(const char *path)
{
    int channels;
    unsigned char *data = stbi_load(path, &img_w, &img_h, &channels, 4);
    if (!data) {
        fprintf(stderr, "vgp-view: cannot load %s: %s\n", path, stbi_failure_reason());
        return -1;
    }

    /* stb_image gives us RGBA. VGP surface_attach expects ARGB (BGRA in memory).
     * Swizzle R<->B for the server. */
    pixels = malloc((size_t)img_w * (size_t)img_h * 4);
    if (!pixels) { stbi_image_free(data); return -1; }

    for (int i = 0; i < img_w * img_h; i++) {
        pixels[i * 4 + 0] = data[i * 4 + 2]; /* B */
        pixels[i * 4 + 1] = data[i * 4 + 1]; /* G */
        pixels[i * 4 + 2] = data[i * 4 + 0]; /* R */
        pixels[i * 4 + 3] = data[i * 4 + 3]; /* A */
    }

    stbi_image_free(data);
    fprintf(stderr, "vgp-view: loaded %s (%dx%d)\n", path, img_w, img_h);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: vgp-view <image-file>\n");
        return 1;
    }

    FILE *logfile = fopen("/tmp/vgp-view.log", "w");
    if (logfile) { setvbuf(logfile, NULL, _IOLBF, 0); dup2(fileno(logfile), STDERR_FILENO); fclose(logfile); }

    signal(SIGPIPE, SIG_IGN);

    if (load_image(argv[1]) < 0)
        return 1;

    conn = vgp_connect(NULL);
    if (!conn) { fprintf(stderr, "vgp-view: cannot connect\n"); return 1; }

    vgp_set_event_callback(conn, on_event, NULL);

    /* Create window sized to image (capped at reasonable size) */
    uint32_t win_w = (uint32_t)img_w;
    uint32_t win_h = (uint32_t)img_h;
    if (win_w > 1600) win_w = 1600;
    if (win_h > 1000) win_h = 1000;

    /* Extract filename for title */
    const char *filename = strrchr(argv[1], '/');
    filename = filename ? filename + 1 : argv[1];
    char title[128];
    snprintf(title, sizeof(title), "%s (%dx%d)", filename, img_w, img_h);

    window_id = vgp_window_create(conn, -1, -1, win_w, win_h, title,
                                    VGP_WINDOW_DECORATED | VGP_WINDOW_RESIZABLE);
    if (!window_id) { fprintf(stderr, "vgp-view: cannot create window\n"); return 1; }

    /* Send image as pixel surface */
    vgp_surface_attach(conn, window_id, (uint32_t)img_w, (uint32_t)img_h,
                        (uint32_t)img_w * 4, pixels);

    /* Event loop */
    while (running) {
        struct pollfd pfd = { .fd = vgp_fd(conn), .events = POLLIN };
        int ret = poll(&pfd, 1, -1);
        if (ret > 0 && (pfd.revents & POLLIN)) {
            if (vgp_dispatch(conn) < 0) break;
        }
    }

    vgp_window_destroy(conn, window_id);
    vgp_disconnect(conn);
    free(pixels);
    return 0;
}
