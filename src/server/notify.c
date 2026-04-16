#include "notify.h"
#include "render_backend.h"
#include "vgp/log.h"

#include <dbus/dbus.h>
#include <string.h>
#include <sys/epoll.h>

#define TAG "notify"

/* ============================================================
 * D-Bus message handlers
 * ============================================================ */

static DBusHandlerResult handle_notify(vgp_notify_t *n, DBusConnection *conn,
                                        DBusMessage *msg)
{
    const char *app_name = "";
    uint32_t replaces_id = 0;
    const char *app_icon = "";
    const char *summary = "";
    const char *body = "";
    int32_t timeout = -1;

    DBusMessageIter args;
    if (dbus_message_iter_init(msg, &args)) {
        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
            dbus_message_iter_get_basic(&args, &app_name);
        dbus_message_iter_next(&args);

        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_UINT32)
            dbus_message_iter_get_basic(&args, &replaces_id);
        dbus_message_iter_next(&args);

        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
            dbus_message_iter_get_basic(&args, &app_icon);
        dbus_message_iter_next(&args);

        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
            dbus_message_iter_get_basic(&args, &summary);
        dbus_message_iter_next(&args);

        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_STRING)
            dbus_message_iter_get_basic(&args, &body);
        dbus_message_iter_next(&args);

        /* Skip actions array */
        dbus_message_iter_next(&args);
        /* Skip hints dict */
        dbus_message_iter_next(&args);

        if (dbus_message_iter_get_arg_type(&args) == DBUS_TYPE_INT32)
            dbus_message_iter_get_basic(&args, &timeout);
    }

    (void)app_icon;

    /* Create notification */
    uint32_t id = replaces_id > 0 ? replaces_id : n->next_id++;

    /* Find slot (replace existing or use first free) */
    vgp_notification_t *notif = NULL;
    if (replaces_id > 0) {
        for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++) {
            if (n->notifications[i].active && n->notifications[i].id == replaces_id) {
                notif = &n->notifications[i];
                break;
            }
        }
    }
    if (!notif) {
        for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++) {
            if (!n->notifications[i].active) {
                notif = &n->notifications[i];
                break;
            }
        }
    }
    if (!notif) {
        /* Evict oldest */
        notif = &n->notifications[0];
        for (int i = 1; i < VGP_MAX_NOTIFICATIONS; i++) {
            if (n->notifications[i].age > notif->age)
                notif = &n->notifications[i];
        }
    }

    notif->id = id;
    snprintf(notif->summary, sizeof(notif->summary), "%s", summary);
    snprintf(notif->body, sizeof(notif->body), "%s", body);
    snprintf(notif->app_name, sizeof(notif->app_name), "%s", app_name);
    notif->timeout = timeout > 0 ? (float)timeout / 1000.0f : 5.0f;
    notif->age = 0;
    notif->active = true;

    /* Recount */
    n->count = 0;
    for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++)
        if (n->notifications[i].active) n->count++;

    VGP_LOG_INFO(TAG, "notification %u: \"%s\" - \"%s\" (%.1fs)",
                 id, summary, body, notif->timeout);

    /* Reply with the notification ID */
    DBusMessage *reply = dbus_message_new_method_return(msg);
    dbus_message_append_args(reply, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);

    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_get_capabilities(DBusConnection *conn,
                                                   DBusMessage *msg)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    DBusMessageIter args, arr;
    dbus_message_iter_init_append(reply, &args);
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "s", &arr);
    const char *caps[] = { "body", "body-markup" };
    for (int i = 0; i < 2; i++)
        dbus_message_iter_append_basic(&arr, DBUS_TYPE_STRING, &caps[i]);
    dbus_message_iter_close_container(&args, &arr);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_get_server_info(DBusConnection *conn,
                                                  DBusMessage *msg)
{
    DBusMessage *reply = dbus_message_new_method_return(msg);
    const char *name = "VGP";
    const char *vendor = "VGP Project";
    const char *version = "0.1.0";
    const char *spec = "1.2";
    dbus_message_append_args(reply,
        DBUS_TYPE_STRING, &name,
        DBUS_TYPE_STRING, &vendor,
        DBUS_TYPE_STRING, &version,
        DBUS_TYPE_STRING, &spec,
        DBUS_TYPE_INVALID);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult handle_close(vgp_notify_t *n, DBusConnection *conn,
                                       DBusMessage *msg)
{
    uint32_t id = 0;
    dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &id, DBUS_TYPE_INVALID);

    for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++) {
        if (n->notifications[i].active && n->notifications[i].id == id) {
            n->notifications[i].active = false;
            n->count--;
            break;
        }
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static DBusHandlerResult dbus_filter(DBusConnection *conn, DBusMessage *msg,
                                      void *user_data)
{
    vgp_notify_t *n = user_data;

    /* Only handle method calls on our interface */
    int msg_type = dbus_message_get_type(msg);
    if (msg_type != DBUS_MESSAGE_TYPE_METHOD_CALL)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    const char *iface = dbus_message_get_interface(msg);
    if (!iface || strcmp(iface, "org.freedesktop.Notifications") != 0)
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    const char *member = dbus_message_get_member(msg);
    if (!member) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (strcmp(member, "Notify") == 0)
        return handle_notify(n, conn, msg);
    if (strcmp(member, "GetCapabilities") == 0)
        return handle_get_capabilities(conn, msg);
    if (strcmp(member, "GetServerInformation") == 0)
        return handle_get_server_info(conn, msg);
    if (strcmp(member, "CloseNotification") == 0)
        return handle_close(n, conn, msg);

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

int vgp_notify_init(vgp_notify_t *notify, vgp_event_loop_t *loop)
{
    memset(notify, 0, sizeof(*notify));
    notify->next_id = 1;

    DBusError err;
    dbus_error_init(&err);

    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn || dbus_error_is_set(&err)) {
        VGP_LOG_WARN(TAG, "D-Bus session bus not available: %s",
                     err.message ? err.message : "unknown");
        dbus_error_free(&err);
        return -1;
    }

    /* Claim the notification service name */
    int ret = dbus_bus_request_name(conn, "org.freedesktop.Notifications",
                                     DBUS_NAME_FLAG_REPLACE_EXISTING |
                                     DBUS_NAME_FLAG_DO_NOT_QUEUE, &err);
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        VGP_LOG_WARN(TAG, "cannot claim org.freedesktop.Notifications: %s",
                     err.message ? err.message : "already owned");
        dbus_error_free(&err);
        /* Continue anyway -- we'll still try to handle messages */
    }

    /* Add message filter */
    dbus_connection_add_filter(conn, dbus_filter, notify, NULL);

    /* Get the D-Bus fd for epoll integration */
    int dbus_fd = -1;
    if (!dbus_connection_get_unix_fd(conn, &dbus_fd) || dbus_fd < 0) {
        VGP_LOG_WARN(TAG, "cannot get D-Bus fd for epoll");
        notify->dbus_conn = conn;
        notify->initialized = true;
        return 0; /* still usable, just poll-based */
    }

    notify->dbus_conn = conn;
    notify->dbus_fd = dbus_fd;
    notify->dbus_source.type = VGP_EVENT_TIMER; /* reuse timer type for dispatch */
    notify->dbus_source.fd = dbus_fd;
    notify->dbus_source.data = notify;

    vgp_event_loop_add_fd(loop, dbus_fd, EPOLLIN, &notify->dbus_source);

    notify->initialized = true;
    VGP_LOG_INFO(TAG, "notification daemon started (D-Bus)");
    return 0;
}

void vgp_notify_destroy(vgp_notify_t *notify, vgp_event_loop_t *loop)
{
    if (!notify->initialized) return;

    if (notify->dbus_fd >= 0)
        vgp_event_loop_del_fd(loop, notify->dbus_fd);

    if (notify->dbus_conn) {
        dbus_connection_remove_filter(notify->dbus_conn, dbus_filter, notify);
        dbus_connection_unref(notify->dbus_conn);
    }

    notify->initialized = false;
}

void vgp_notify_dispatch(vgp_notify_t *notify)
{
    if (!notify->initialized || !notify->dbus_conn) return;
    DBusConnection *conn = notify->dbus_conn;

    /* Process all pending D-Bus messages */
    while (dbus_connection_dispatch(conn) == DBUS_DISPATCH_DATA_REMAINS)
        ;
    dbus_connection_read_write(conn, 0); /* non-blocking */
}

void vgp_notify_tick(vgp_notify_t *notify, float dt)
{
    for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++) {
        vgp_notification_t *n = &notify->notifications[i];
        if (!n->active) continue;
        n->age += dt;
        n->timeout -= dt;
        if (n->timeout <= 0) {
            n->active = false;
            notify->count--;
        }
    }
}

/* ============================================================
 * Rendering
 * ============================================================ */

void vgp_notify_render(vgp_notify_t *notify, void *backend, void *ctx,
                        float screen_w, float screen_h,
                        float font_size)
{
    vgp_render_backend_t *b = backend;
    float notif_w = 320.0f;
    float notif_h = 70.0f;
    float margin = 10.0f;
    float x = screen_w - notif_w - margin;
    float y = margin;

    for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++) {
        vgp_notification_t *n = &notify->notifications[i];
        if (!n->active) continue;

        /* Fade in/out */
        float alpha = 1.0f;
        if (n->age < 0.3f) alpha = n->age / 0.3f;
        if (n->timeout < 0.5f) alpha = n->timeout / 0.5f;
        if (alpha < 0) alpha = 0;

        /* Background */
        b->ops->draw_rounded_rect(b, ctx, x, y, notif_w, notif_h, 8.0f,
                                   0.12f, 0.12f, 0.18f, 0.9f * alpha);

        /* Left accent bar */
        b->ops->draw_rounded_rect(b, ctx, x, y + 4, 3, notif_h - 8, 1.5f,
                                   0.32f, 0.53f, 0.88f, alpha);

        /* Summary (bold-ish) */
        b->ops->draw_text(b, ctx, n->summary, -1, x + 12, y + 22,
                           font_size, 0.92f, 0.92f, 0.92f, alpha);

        /* Body */
        if (n->body[0])
            b->ops->draw_text(b, ctx, n->body, -1, x + 12, y + 44,
                               font_size - 2, 0.65f, 0.65f, 0.65f, alpha);

        /* App name (small, right-aligned) */
        if (n->app_name[0])
            b->ops->draw_text(b, ctx, n->app_name, -1,
                               x + notif_w - 80, y + 22,
                               font_size - 3, 0.4f, 0.4f, 0.5f, alpha);

        y += notif_h + margin;
    }
}

bool vgp_notify_click(vgp_notify_t *notify, float click_x, float click_y,
                       float screen_w, float screen_h)
{
    (void)screen_h;
    float notif_w = 320.0f;
    float notif_h = 70.0f;
    float margin = 10.0f;
    float x = screen_w - notif_w - margin;
    float y = margin;

    for (int i = 0; i < VGP_MAX_NOTIFICATIONS; i++) {
        vgp_notification_t *n = &notify->notifications[i];
        if (!n->active) continue;

        if (click_x >= x && click_x < x + notif_w &&
            click_y >= y && click_y < y + notif_h) {
            n->active = false;
            notify->count--;
            return true;
        }
        y += notif_h + margin;
    }
    return false;
}
