/* SPDX-License-Identifier: MIT */
#include "calendar.h"
#include <stdio.h>
#include <string.h>

void vgp_calendar_init(vgp_calendar_t *cal)
{
    memset(cal, 0, sizeof(*cal));
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    cal->year = tm->tm_year + 1900;
    cal->month = tm->tm_mon + 1;
}

void vgp_calendar_toggle(vgp_calendar_t *cal, float x, float y)
{
    cal->visible = !cal->visible;
    cal->x = x;
    cal->y = y;
    if (cal->visible) {
        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        cal->year = tm->tm_year + 1900;
        cal->month = tm->tm_mon + 1;
    }
}

static int days_in_month(int year, int month)
{
    static const int d[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};
    int days = d[month];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        days = 29;
    return days;
}

static int day_of_week(int year, int month, int day)
{
    /* Tomohiko Sakamoto's algorithm */
    static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year--;
    return (year + year/4 - year/100 + year/400 + t[month-1] + day) % 7;
}

void vgp_calendar_render(vgp_calendar_t *cal, vgp_render_backend_t *b, void *ctx,
                          float font_size)
{
    if (!cal->visible) return;

    float w = 220, h = 200;
    float x = cal->x - w;
    float y = cal->y;

    /* Background */
    b->ops->draw_rounded_rect(b, ctx, x + 4, y + 4, w, h, 8, 0, 0, 0, 0.3f);
    b->ops->draw_rounded_rect(b, ctx, x, y, w, h, 8, 0.1f, 0.1f, 0.15f, 0.95f);
    b->ops->draw_rounded_rect(b, ctx, x, y, w, h, 8, 0.3f, 0.3f, 0.4f, 0.2f);

    /* Month/year header */
    static const char *months[] = {"","Jan","Feb","Mar","Apr","May","Jun",
                                    "Jul","Aug","Sep","Oct","Nov","Dec"};
    char header[32];
    snprintf(header, sizeof(header), "%s %d", months[cal->month], cal->year);
    b->ops->draw_text(b, ctx, header, -1, x + w/2 - 30, y + 24, font_size + 1,
                       0.85f, 0.85f, 0.85f, 1.0f);

    /* Day of week headers */
    static const char *dow[] = {"Su","Mo","Tu","We","Th","Fr","Sa"};
    float cell_w = w / 7.0f;
    for (int i = 0; i < 7; i++) {
        b->ops->draw_text(b, ctx, dow[i], -1,
                           x + 4 + (float)i * cell_w, y + 48, font_size - 2,
                           0.5f, 0.5f, 0.6f, 1.0f);
    }

    /* Days grid */
    int days = days_in_month(cal->year, cal->month);
    int start_dow = day_of_week(cal->year, cal->month, 1);

    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    int today = tm_now->tm_mday;
    bool is_current_month = (tm_now->tm_year + 1900 == cal->year &&
                              tm_now->tm_mon + 1 == cal->month);

    for (int d = 1; d <= days; d++) {
        int pos = start_dow + d - 1;
        int row = pos / 7;
        int col = pos % 7;
        float dx = x + 6 + (float)col * cell_w;
        float dy = y + 64 + (float)row * 20;

        char day_str[4];
        snprintf(day_str, sizeof(day_str), "%d", d);

        bool is_today = is_current_month && d == today;
        if (is_today) {
            b->ops->draw_circle(b, ctx, dx + 8, dy - 4, 10,
                                 0.32f, 0.53f, 0.88f, 0.8f);
            b->ops->draw_text(b, ctx, day_str, -1, dx + 2, dy, font_size - 2,
                               1.0f, 1.0f, 1.0f, 1.0f);
        } else {
            b->ops->draw_text(b, ctx, day_str, -1, dx + 2, dy, font_size - 2,
                               0.7f, 0.7f, 0.7f, 1.0f);
        }
    }
}