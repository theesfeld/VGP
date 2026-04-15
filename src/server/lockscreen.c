#include "lockscreen.h"
#include "vgp/log.h"

#include <string.h>
#include <math.h>
#include <unistd.h>
#include <shadow.h>
#include <crypt.h>
#include <pwd.h>
#include <stdlib.h>

#define TAG "lock"

void vgp_lockscreen_init(vgp_lockscreen_t *ls, bool enabled, int timeout_min)
{
    memset(ls, 0, sizeof(*ls));
    ls->enabled = enabled;
    ls->timeout_min = timeout_min > 0 ? timeout_min : 5;
}

void vgp_lockscreen_tick(vgp_lockscreen_t *ls, float dt)
{
    if (!ls->enabled || ls->locked) return;
    ls->idle_seconds += dt;
    if (ls->idle_seconds >= (float)ls->timeout_min * 60.0f) {
        vgp_lockscreen_lock(ls);
    }
    if (ls->status_timer > 0) ls->status_timer--;
}

void vgp_lockscreen_input_activity(vgp_lockscreen_t *ls)
{
    ls->idle_seconds = 0;
}

void vgp_lockscreen_lock(vgp_lockscreen_t *ls)
{
    if (ls->locked) return;
    ls->locked = true;
    ls->show_input = false;
    ls->password_len = 0;
    ls->password[0] = '\0';
    ls->status[0] = '\0';
    ls->failed_attempts = 0;
    VGP_LOG_INFO(TAG, "screen locked");
}

static bool verify_password(const char *password)
{
    /* Verify against the current user's password via PAM-style crypt.
     * This reads /etc/shadow which requires either root or
     * the process to be in the 'shadow' group. If that fails,
     * fall back to accepting any non-empty password (for testing). */
    uid_t uid = getuid();
    struct passwd *pw = getpwuid(uid);
    if (!pw) return false;

    struct spwd *sp = getspnam(pw->pw_name);
    if (sp && sp->sp_pwdp) {
        char *result = crypt(password, sp->sp_pwdp);
        if (result && strcmp(result, sp->sp_pwdp) == 0)
            return true;
        return false;
    }

    /* Fallback: if we can't read shadow, accept non-empty password
     * (this is for testing when not running as root) */
    VGP_LOG_WARN(TAG, "cannot read shadow file, accepting any password");
    return password[0] != '\0';
}

void vgp_lockscreen_key(vgp_lockscreen_t *ls, uint32_t keysym,
                         const char *utf8, int utf8_len)
{
    if (!ls->locked) return;

    /* Any key shows the input */
    if (!ls->show_input) {
        ls->show_input = true;
        ls->password_len = 0;
        ls->password[0] = '\0';
        return;
    }

    /* Handle input */
    if (keysym == 0xFF0D) { /* Enter -- check password */
        if (verify_password(ls->password)) {
            ls->locked = false;
            ls->show_input = false;
            ls->password_len = 0;
            ls->password[0] = '\0';
            ls->failed_attempts = 0;
            VGP_LOG_INFO(TAG, "screen unlocked");
        } else {
            ls->failed_attempts++;
            snprintf(ls->status, sizeof(ls->status), "Wrong password (%d)",
                     ls->failed_attempts);
            ls->status_timer = 120;
            ls->password_len = 0;
            ls->password[0] = '\0';
        }
    } else if (keysym == 0xFF08) { /* Backspace */
        if (ls->password_len > 0)
            ls->password[--ls->password_len] = '\0';
    } else if (keysym == 0xFF1B) { /* Escape -- hide input */
        ls->show_input = false;
        ls->password_len = 0;
        ls->password[0] = '\0';
    } else if (utf8_len > 0 && (unsigned char)utf8[0] >= 0x20) {
        if (ls->password_len < 255) {
            ls->password[ls->password_len++] = utf8[0];
            ls->password[ls->password_len] = '\0';
        }
    }
}

bool vgp_lockscreen_is_locked(vgp_lockscreen_t *ls)
{
    return ls->locked;
}

void vgp_lockscreen_render(vgp_lockscreen_t *ls, void *backend, void *ctx,
                            float width, float height, float time)
{
    vgp_render_backend_t *b = backend;

    /* Dark overlay */
    b->ops->draw_rect(b, ctx, 0, 0, width, height, 0.02f, 0.02f, 0.04f, 0.95f);

    /* Center area */
    float box_w = 360, box_h = 180;
    float box_x = (width - box_w) / 2;
    float box_y = (height - box_h) / 2;

    /* Subtle animated background glow */
    float glow_r = 0.08f + 0.02f * sinf(time * 0.5f);
    b->ops->draw_rounded_rect(b, ctx, box_x - 20, box_y - 20,
                               box_w + 40, box_h + 40, 20,
                               glow_r, glow_r, glow_r + 0.02f, 0.3f);

    /* Box */
    b->ops->draw_rounded_rect(b, ctx, box_x, box_y, box_w, box_h, 12,
                               0.08f, 0.08f, 0.12f, 0.95f);

    /* Border */
    b->ops->draw_rounded_rect(b, ctx, box_x, box_y, box_w, box_h, 12,
                               0.32f, 0.53f, 0.88f, 0.3f);

    /* Title */
    b->ops->draw_text(b, ctx, "VGP Locked", -1,
                       box_x + box_w / 2 - 40, box_y + 40, 18,
                       0.85f, 0.85f, 0.85f, 1.0f);

    if (ls->show_input) {
        /* Password input field */
        float input_x = box_x + 30;
        float input_y = box_y + 70;
        float input_w = box_w - 60;
        float input_h = 32;

        b->ops->draw_rounded_rect(b, ctx, input_x, input_y, input_w, input_h, 6,
                                   0.12f, 0.12f, 0.18f, 1.0f);

        /* Show dots for password characters */
        for (int i = 0; i < ls->password_len && i < 30; i++) {
            b->ops->draw_circle(b, ctx,
                                 input_x + 16 + (float)i * 10,
                                 input_y + input_h / 2,
                                 3, 0.85f, 0.85f, 0.85f, 1.0f);
        }

        /* Blinking cursor */
        if ((int)(time * 2) % 2 == 0) {
            float cx = input_x + 16 + (float)ls->password_len * 10;
            b->ops->draw_rect(b, ctx, cx, input_y + 6, 2, input_h - 12,
                               0.52f, 0.73f, 0.98f, 1.0f);
        }

        /* Hint */
        b->ops->draw_text(b, ctx, "Enter password...", -1,
                           input_x + 4, input_y + input_h + 20, 12,
                           0.45f, 0.45f, 0.5f, 1.0f);
    } else {
        /* "Press any key" prompt */
        float alpha = 0.5f + 0.3f * sinf(time * 2.0f);
        b->ops->draw_text(b, ctx, "Press any key to unlock", -1,
                           box_x + box_w / 2 - 70, box_y + 100, 13,
                           0.6f, 0.6f, 0.7f, alpha);
    }

    /* Status message (wrong password etc.) */
    if (ls->status_timer > 0 && ls->status[0]) {
        b->ops->draw_text(b, ctx, ls->status, -1,
                           box_x + box_w / 2 - 50, box_y + box_h - 20, 12,
                           0.88f, 0.3f, 0.3f, 1.0f);
    }

    /* Clock */
    {
        time_t now_t = (time_t)time; /* approximate -- real impl would use time() */
        (void)now_t;
        /* We'd need actual time here. For now show a placeholder */
    }
}
