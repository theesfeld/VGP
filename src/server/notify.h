#ifndef VGP_NOTIFY_H
#define VGP_NOTIFY_H

#include "loop.h"
#include <stdbool.h>
#include <stdint.h>

#define VGP_MAX_NOTIFICATIONS 16
#define VGP_NOTIFY_MAX_TEXT   256

typedef struct vgp_notification {
    uint32_t id;
    char     summary[VGP_NOTIFY_MAX_TEXT];
    char     body[VGP_NOTIFY_MAX_TEXT];
    char     app_name[64];
    float    timeout;      /* seconds remaining */
    float    age;          /* seconds since creation */
    bool     active;
} vgp_notification_t;

typedef struct vgp_notify {
    vgp_notification_t notifications[VGP_MAX_NOTIFICATIONS];
    int                count;
    uint32_t           next_id;

    /* D-Bus connection */
    void              *dbus_conn;  /* DBusConnection* */
    int                dbus_fd;
    vgp_event_source_t dbus_source;
    bool               initialized;
} vgp_notify_t;

struct vgp_server;

int  vgp_notify_init(vgp_notify_t *notify, vgp_event_loop_t *loop);
void vgp_notify_destroy(vgp_notify_t *notify, vgp_event_loop_t *loop);
void vgp_notify_dispatch(vgp_notify_t *notify);
void vgp_notify_tick(vgp_notify_t *notify, float dt);

/* Render notifications (call from renderer, top-right corner) */
void vgp_notify_render(vgp_notify_t *notify, void *backend, void *ctx,
                        float screen_w, float screen_h,
                        float font_size);

#endif /* VGP_NOTIFY_H */
