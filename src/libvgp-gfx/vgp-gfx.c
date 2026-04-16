/* VGP Graphical UI Toolkit -- Core implementation
 * Command buffer management, event handling, lifecycle. */

#include "vgp-gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <signal.h>
#include <time.h>

/* ============================================================
 * Command buffer helpers
 * ============================================================ */

static void cmd_ensure(vgfx_ctx_t *ctx, size_t need)
{
    if (ctx->cmd_len + need <= ctx->cmd_cap) return;
    size_t new_cap = ctx->cmd_cap * 2;
    if (new_cap < ctx->cmd_len + need) new_cap = ctx->cmd_len + need + 4096;
    ctx->cmd_buf = realloc(ctx->cmd_buf, new_cap);
    ctx->cmd_cap = new_cap;
}

static void cmd_u8(vgfx_ctx_t *ctx, uint8_t v)
{
    cmd_ensure(ctx, 1);
    ctx->cmd_buf[ctx->cmd_len++] = v;
}

static void cmd_f32(vgfx_ctx_t *ctx, float v)
{
    cmd_ensure(ctx, 4);
    memcpy(ctx->cmd_buf + ctx->cmd_len, &v, 4);
    ctx->cmd_len += 4;
}

static void cmd_u16(vgfx_ctx_t *ctx, uint16_t v)
{
    cmd_ensure(ctx, 2);
    memcpy(ctx->cmd_buf + ctx->cmd_len, &v, 2);
    ctx->cmd_len += 2;
}

static void cmd_bytes(vgfx_ctx_t *ctx, const void *data, size_t len)
{
    cmd_ensure(ctx, len);
    memcpy(ctx->cmd_buf + ctx->cmd_len, data, len);
    ctx->cmd_len += len;
}

static void cmd_color(vgfx_ctx_t *ctx, vgfx_color_t c)
{
    cmd_f32(ctx, c.r); cmd_f32(ctx, c.g); cmd_f32(ctx, c.b); cmd_f32(ctx, c.a);
}

/* ============================================================
 * Event handler
 * ============================================================ */

static void on_event(vgp_connection_t *conn, const vgp_event_t *ev, void *data)
{
    vgfx_ctx_t *ctx = data;
    (void)conn;

    switch (ev->type) {
    case VGP_EVENT_KEY_PRESS:
        ctx->last_keysym = ev->key.keysym;
        ctx->last_mods = ev->key.modifiers;
        memcpy(ctx->last_utf8, ev->key.utf8, sizeof(ctx->last_utf8));
        ctx->key_pressed = true;
        ctx->dirty = true;
        break;
    case VGP_EVENT_MOUSE_BUTTON:
        ctx->mouse_x = ev->mouse_button.x;
        ctx->mouse_y = ev->mouse_button.y;
        if (ev->mouse_button.pressed && !ctx->mouse_pressed)
            ctx->mouse_clicked = true;
        ctx->mouse_pressed = ev->mouse_button.pressed;
        ctx->dirty = true;
        break;
    case VGP_EVENT_MOUSE_MOVE:
        ctx->mouse_x = ev->mouse_move.x;
        ctx->mouse_y = ev->mouse_move.y;
        ctx->dirty = true;
        break;
    case VGP_EVENT_MOUSE_SCROLL:
        ctx->scroll_dy += ev->scroll.dy;
        ctx->dirty = true;
        break;
    case VGP_EVENT_CONFIGURE:
        ctx->width = (float)ev->configure.width;
        ctx->height = (float)ev->configure.height;
        ctx->dirty = true;
        break;
    case VGP_EVENT_CLOSE:
        ctx->running = false;
        break;
    case VGP_EVENT_THEME_INFO:
        memcpy(ctx->theme.colors, ev->theme.colors, sizeof(ev->theme.colors));
        ctx->theme.font_size = ev->theme.font_size;
        ctx->theme.font_size_small = ev->theme.font_size_small;
        ctx->theme.font_size_large = ev->theme.font_size_large;
        ctx->theme.corner_radius = ev->theme.corner_radius;
        ctx->theme.padding = ev->theme.padding;
        ctx->theme.spacing = ev->theme.spacing;
        ctx->theme.border_width = ev->theme.border_width;
        ctx->theme.scrollbar_width = ev->theme.scrollbar_width;
        ctx->theme.button_height = ev->theme.button_height;
        ctx->theme.input_height = ev->theme.input_height;
        ctx->theme.checkbox_size = ev->theme.checkbox_size;
        ctx->theme.slider_height = ev->theme.slider_height;
        memcpy(ctx->theme.char_advances, ev->theme.char_advances,
               sizeof(ev->theme.char_advances));
        ctx->theme.received = true;
        ctx->dirty = true;
        break;
    default:
        break;
    }
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

int vgfx_init(vgfx_ctx_t *ctx, const char *title, int width, int height, uint32_t flags)
{
    memset(ctx, 0, sizeof(*ctx));
    signal(SIGPIPE, SIG_IGN);

    /* Default theme values (overridden when server sends THEME_INFO) */
    ctx->theme.font_size = 14.0f;
    ctx->theme.font_size_small = 12.0f;
    ctx->theme.font_size_large = 18.0f;
    ctx->theme.corner_radius = 6.0f;
    ctx->theme.padding = 8.0f;
    ctx->theme.spacing = 6.0f;
    ctx->theme.border_width = 1.0f;
    ctx->theme.scrollbar_width = 8.0f;
    ctx->theme.button_height = 28.0f;
    ctx->theme.input_height = 26.0f;
    ctx->theme.checkbox_size = 18.0f;
    ctx->theme.slider_height = 6.0f;
    for (int i = 0; i < 95; i++)
        ctx->theme.char_advances[i] = 8.4f; /* monospace default */

    /* Default dark theme colors */
    float dc[][4] = {
        {0.10f,0.10f,0.18f,1}, /* BG */
        {0.13f,0.13f,0.21f,1}, /* BG_SECONDARY */
        {0.17f,0.17f,0.25f,1}, /* BG_TERTIARY */
        {0.88f,0.88f,0.88f,1}, /* FG */
        {0.55f,0.55f,0.60f,1}, /* FG_SECONDARY */
        {0.35f,0.35f,0.40f,1}, /* FG_DISABLED */
        {0.32f,0.58f,0.88f,1}, /* ACCENT */
        {0.40f,0.65f,0.95f,1}, /* ACCENT_HOVER */
        {0.24f,0.24f,0.30f,1}, /* BORDER */
        {0.88f,0.38f,0.38f,1}, /* ERROR */
        {0.38f,0.75f,0.38f,1}, /* SUCCESS */
        {0.88f,0.75f,0.25f,1}, /* WARNING */
        {0.14f,0.14f,0.20f,1}, /* SCROLLBAR */
        {0.40f,0.40f,0.50f,1}, /* SCROLLBAR_THUMB */
        {0.32f,0.58f,0.88f,0.3f}, /* SELECTION */
        {0.19f,0.19f,0.25f,0.95f}, /* TOOLTIP_BG */
    };
    memcpy(ctx->theme.colors, dc, sizeof(dc));

    /* Allocate command buffer */
    ctx->cmd_cap = VGFX_CMD_BUF_INIT;
    ctx->cmd_buf = malloc(ctx->cmd_cap);
    if (!ctx->cmd_buf) return -1;

    /* Connect to server */
    ctx->conn = vgp_connect(NULL);
    if (!ctx->conn) {
        fprintf(stderr, "vgfx_init: vgp_connect failed\n");
        free(ctx->cmd_buf);
        return -1;
    }

    vgp_set_event_callback(ctx->conn, on_event, ctx);

    /* Don't force DECORATED on override windows */
    uint32_t win_flags = flags;
    if (!(flags & VGP_WINDOW_OVERRIDE))
        win_flags |= VGP_WINDOW_DECORATED | VGP_WINDOW_RESIZABLE;

    ctx->window_id = vgp_window_create(ctx->conn, -1, -1,
                                         (uint32_t)width, (uint32_t)height,
                                         title, win_flags);
    if (!ctx->window_id) {
        fprintf(stderr, "vgfx_init: window_create failed\n");
        vgp_disconnect(ctx->conn);
        free(ctx->cmd_buf);
        return -1;
    }

    ctx->width = (float)width;
    ctx->height = (float)height;
    ctx->running = true;
    ctx->dirty = true;

    /* Process events to receive theme info */
    vgfx_poll(ctx, 100);

    return 0;
}

void vgfx_destroy(vgfx_ctx_t *ctx)
{
    if (ctx->window_id && ctx->conn)
        vgp_window_destroy(ctx->conn, ctx->window_id);
    if (ctx->conn)
        vgp_disconnect(ctx->conn);
    free(ctx->cmd_buf);
}

void vgfx_begin_frame(vgfx_ctx_t *ctx)
{
    ctx->cmd_len = 0;
    ctx->cmd_count = 0;
    ctx->next_id = 1;
    ctx->hot_id = 0;
}

void vgfx_end_frame(vgfx_ctx_t *ctx)
{
    /* Send draw commands to server */
    if (ctx->cmd_len > 0) {
        vgp_draw_commands_send(ctx->conn, ctx->window_id,
                                ctx->cmd_buf, ctx->cmd_len, ctx->cmd_count);
    }
    /* Clear per-frame input */
    ctx->key_pressed = false;
    ctx->mouse_clicked = false;
    ctx->scroll_dy = 0;
}

int vgfx_poll(vgfx_ctx_t *ctx, int timeout_ms)
{
    struct pollfd pfd = { .fd = vgp_fd(ctx->conn), .events = POLLIN };
    int ret = poll(&pfd, 1, timeout_ms);
    if (ret > 0 && (pfd.revents & POLLIN)) {
        if (vgp_dispatch(ctx->conn) < 0) {
            ctx->running = false;
            return -1;
        }
    }
    return ret;
}

void vgfx_run(vgfx_ctx_t *ctx, void (*render)(vgfx_ctx_t *ctx))
{
    while (ctx->running) {
        vgfx_poll(ctx, 16);
        if (ctx->dirty) {
            vgfx_begin_frame(ctx);
            render(ctx);
            vgfx_end_frame(ctx);
            ctx->dirty = false;
        }
    }
}

/* ============================================================
 * Drawing primitives
 * ============================================================ */

void vgfx_clear(vgfx_ctx_t *ctx, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_CLEAR);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_rect(vgfx_ctx_t *ctx, float x, float y, float w, float h, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_RECT);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, w); cmd_f32(ctx, h);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_rounded_rect(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                         float radius, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_ROUNDED_RECT);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, w); cmd_f32(ctx, h);
    cmd_f32(ctx, radius);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_circle(vgfx_ctx_t *ctx, float cx, float cy, float r, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_CIRCLE);
    cmd_f32(ctx, cx); cmd_f32(ctx, cy); cmd_f32(ctx, r);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_line(vgfx_ctx_t *ctx, float x1, float y1, float x2, float y2,
                float width, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_LINE);
    cmd_f32(ctx, x1); cmd_f32(ctx, y1); cmd_f32(ctx, x2); cmd_f32(ctx, y2);
    cmd_f32(ctx, width);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_text(vgfx_ctx_t *ctx, const char *text, float x, float y,
                float size, vgfx_color_t c)
{
    int len = (int)strlen(text);
    if (len > 65535) len = 65535;
    cmd_u8(ctx, VGP_DCMD_TEXT);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, size);
    cmd_color(ctx, c);
    cmd_u16(ctx, (uint16_t)len);
    cmd_bytes(ctx, text, (size_t)len);
    ctx->cmd_count++;
}

void vgfx_text_bold(vgfx_ctx_t *ctx, const char *text, float x, float y,
                      float size, vgfx_color_t c)
{
    int len = (int)strlen(text);
    if (len > 65535) len = 65535;
    cmd_u8(ctx, VGP_DCMD_TEXT_BOLD);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, size);
    cmd_color(ctx, c);
    cmd_u16(ctx, (uint16_t)len);
    cmd_bytes(ctx, text, (size_t)len);
    ctx->cmd_count++;
}

void vgfx_rect_outline(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                         float line_w, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_RECT_OUTLINE);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, w); cmd_f32(ctx, h);
    cmd_f32(ctx, line_w);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_rounded_rect_outline(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                                  float radius, float line_w, vgfx_color_t c)
{
    cmd_u8(ctx, VGP_DCMD_RRECT_OUTLINE);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, w); cmd_f32(ctx, h);
    cmd_f32(ctx, radius); cmd_f32(ctx, line_w);
    cmd_color(ctx, c);
    ctx->cmd_count++;
}

void vgfx_gradient_rect(vgfx_ctx_t *ctx, float x, float y, float w, float h,
                          vgfx_color_t top, vgfx_color_t bottom)
{
    cmd_u8(ctx, VGP_DCMD_GRADIENT_RECT);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, w); cmd_f32(ctx, h);
    cmd_color(ctx, top);
    cmd_color(ctx, bottom);
    ctx->cmd_count++;
}

void vgfx_push_clip(vgfx_ctx_t *ctx, float x, float y, float w, float h)
{
    cmd_u8(ctx, VGP_DCMD_PUSH_STATE);
    ctx->cmd_count++;
    cmd_u8(ctx, VGP_DCMD_SET_CLIP);
    cmd_f32(ctx, x); cmd_f32(ctx, y); cmd_f32(ctx, w); cmd_f32(ctx, h);
    ctx->cmd_count++;
}

void vgfx_pop_clip(vgfx_ctx_t *ctx)
{
    cmd_u8(ctx, VGP_DCMD_POP_STATE);
    ctx->cmd_count++;
}

/* ============================================================
 * Theme access
 * ============================================================ */

vgfx_color_t vgfx_theme_color(vgfx_ctx_t *ctx, int slot)
{
    if (slot < 0 || slot >= 16)
        return vgfx_rgba(1, 0, 1, 1); /* magenta = missing */
    float *c = ctx->theme.colors[slot];
    return vgfx_rgba(c[0], c[1], c[2], c[3]);
}

float vgfx_text_width(vgfx_ctx_t *ctx, const char *text, int len, float size)
{
    (void)ctx;
    /* Stroke font: every character is monospace on a 4x7 grid.
     * Advance = (grid_width + 1) * scale, where scale = size / grid_height */
    float scale = size / 7.0f;
    float advance = 5.0f * scale;  /* 4 + 1 spacing */
    int n = len < 0 ? (int)strlen(text) : len;
    return (float)n * advance;
}

float vgfx_text_height(vgfx_ctx_t *ctx, float size)
{
    (void)ctx;
    return size * 1.3f; /* approximate line height */
}

void vgfx_run_animated(vgfx_ctx_t *ctx,
                         void (*render)(vgfx_ctx_t *ctx),
                         void (*sample_fn)(void),
                         int sample_interval_ms)
{
    struct timespec last_sample, now, after;
    clock_gettime(CLOCK_MONOTONIC, &last_sample);
    if (sample_fn) sample_fn();

    while (ctx->running) {
        /* Non-blocking event drain */
        vgfx_poll(ctx, 0);

        /* Periodic data sampling */
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed_ms = (now.tv_sec - last_sample.tv_sec) * 1000 +
                          (now.tv_nsec - last_sample.tv_nsec) / 1000000;
        if (sample_fn && elapsed_ms >= sample_interval_ms) {
            sample_fn();
            last_sample = now;
        }

        /* Always render (continuous animation) */
        vgfx_begin_frame(ctx);
        render(ctx);
        vgfx_end_frame(ctx);

        /* Target ~60fps */
        clock_gettime(CLOCK_MONOTONIC, &after);
        long render_us = (after.tv_sec - now.tv_sec) * 1000000 +
                         (after.tv_nsec - now.tv_nsec) / 1000;
        long sleep_us = 16000 - render_us; /* 16ms frame budget */
        if (sleep_us > 1000) {
            struct timespec ts = { .tv_sec = 0, .tv_nsec = sleep_us * 1000 };
            nanosleep(&ts, NULL);
        }
    }
}
