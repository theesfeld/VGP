#include "config-writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES 2048
#define MAX_LINE_LEN 512

typedef struct {
    char lines[MAX_LINES][MAX_LINE_LEN];
    int  count;
} file_buf_t;

static int read_file(file_buf_t *fb, const char *path)
{
    fb->count = 0;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    while (fb->count < MAX_LINES && fgets(fb->lines[fb->count], MAX_LINE_LEN, f))
        fb->count++;
    fclose(f);
    return 0;
}

static int write_file(file_buf_t *fb, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    for (int i = 0; i < fb->count; i++)
        fputs(fb->lines[i], f);
    fclose(f);
    return 0;
}

static void trim(char *s)
{
    while (isspace(*s)) memmove(s, s + 1, strlen(s));
    size_t len = strlen(s);
    while (len > 0 && isspace(s[len - 1])) s[--len] = '\0';
}

/* Find the line index of a [section] header. Returns -1 if not found. */
static int find_section(file_buf_t *fb, const char *section)
{
    char target[128];
    snprintf(target, sizeof(target), "[%s]", section);

    for (int i = 0; i < fb->count; i++) {
        char line[MAX_LINE_LEN];
        snprintf(line, sizeof(line), "%s", fb->lines[i]);
        trim(line);
        if (strcmp(line, target) == 0)
            return i;
    }
    return -1;
}

/* Find a key within a section. Returns line index, or -1. */
static int find_key(file_buf_t *fb, int section_start, const char *key)
{
    for (int i = section_start + 1; i < fb->count; i++) {
        char line[MAX_LINE_LEN];
        snprintf(line, sizeof(line), "%s", fb->lines[i]);
        trim(line);

        /* Stop at next section */
        if (line[0] == '[') break;
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\0') continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char k[128];
        snprintf(k, sizeof(k), "%s", line);
        trim(k);

        if (strcmp(k, key) == 0)
            return i;
    }
    return -1;
}

/* Insert a line at position, shifting everything down */
static void insert_line(file_buf_t *fb, int pos, const char *line)
{
    if (fb->count >= MAX_LINES) return;
    memmove(&fb->lines[pos + 1], &fb->lines[pos],
            (size_t)(fb->count - pos) * MAX_LINE_LEN);
    snprintf(fb->lines[pos], MAX_LINE_LEN, "%s", line);
    fb->count++;
}

int config_set_value(const char *path, const char *section,
                      const char *key, const char *value)
{
    file_buf_t fb;
    if (read_file(&fb, path) < 0) {
        /* File doesn't exist -- create it */
        fb.count = 0;
    }

    /* Find or create section */
    int sec_idx = find_section(&fb, section);
    if (sec_idx < 0) {
        /* Append new section */
        char sec_line[MAX_LINE_LEN];
        snprintf(sec_line, sizeof(sec_line), "\n[%s]\n", section);
        snprintf(fb.lines[fb.count], MAX_LINE_LEN, "%s", sec_line);
        sec_idx = fb.count;
        fb.count++;
    }

    /* Find or create key */
    int key_idx = find_key(&fb, sec_idx, key);
    char new_line[MAX_LINE_LEN];

    /* Quote value if it contains spaces or special chars */
    if (strchr(value, ' ') || strchr(value, ','))
        snprintf(new_line, sizeof(new_line), "%s = \"%s\"\n", key, value);
    else
        snprintf(new_line, sizeof(new_line), "%s = %s\n", key, value);

    if (key_idx >= 0) {
        /* Replace existing line */
        snprintf(fb.lines[key_idx], MAX_LINE_LEN, "%s", new_line);
    } else {
        /* Insert after section header */
        insert_line(&fb, sec_idx + 1, new_line);
    }

    return write_file(&fb, path);
}

int config_set_int(const char *path, const char *section,
                    const char *key, int value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", value);
    return config_set_value(path, section, key, buf);
}

int config_set_float(const char *path, const char *section,
                      const char *key, float value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", value);
    return config_set_value(path, section, key, buf);
}
