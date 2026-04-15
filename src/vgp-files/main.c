/* VGP Files -- Simple file manager */

#include "vgp-ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <time.h>

#define MAX_ENTRIES 1024

typedef struct {
    char  name[256];
    bool  is_dir;
    off_t size;
    time_t mtime;
} file_entry_t;

typedef struct {
    char          cwd[512];
    file_entry_t  entries[MAX_ENTRIES];
    int           entry_count;
    int           selected;
    int           scroll;
} files_state_t;

static files_state_t fs;

static int entry_cmp(const void *a, const void *b)
{
    const file_entry_t *ea = a, *eb = b;
    /* Directories first */
    if (ea->is_dir && !eb->is_dir) return -1;
    if (!ea->is_dir && eb->is_dir) return 1;
    return strcasecmp(ea->name, eb->name);
}

static void scan_dir(void)
{
    fs.entry_count = 0;
    fs.selected = 0;
    fs.scroll = 0;

    DIR *dir = opendir(fs.cwd);
    if (!dir) return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && fs.entry_count < MAX_ENTRIES) {
        if (strcmp(entry->d_name, ".") == 0) continue;

        file_entry_t *fe = &fs.entries[fs.entry_count];
        snprintf(fe->name, sizeof(fe->name), "%s", entry->d_name);

        char full_path[768];
        snprintf(full_path, sizeof(full_path), "%s/%s", fs.cwd, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) == 0) {
            fe->is_dir = S_ISDIR(st.st_mode);
            fe->size = st.st_size;
            fe->mtime = st.st_mtime;
        } else {
            fe->is_dir = false;
            fe->size = 0;
            fe->mtime = 0;
        }

        fs.entry_count++;
    }
    closedir(dir);

    qsort(fs.entries, (size_t)fs.entry_count, sizeof(file_entry_t), entry_cmp);
}

static void navigate(const char *name)
{
    if (strcmp(name, "..") == 0) {
        char *last = strrchr(fs.cwd, '/');
        if (last && last != fs.cwd) *last = '\0';
        else strcpy(fs.cwd, "/");
    } else {
        char new_path[768];
        if (strcmp(fs.cwd, "/") == 0)
            snprintf(new_path, sizeof(new_path), "/%s", name);
        else
            snprintf(new_path, sizeof(new_path), "%s/%s", fs.cwd, name);
        snprintf(fs.cwd, sizeof(fs.cwd), "%s", new_path);
    }
    scan_dir();
}

static const char *format_size(off_t size)
{
    static char buf[32];
    if (size < 1024) snprintf(buf, sizeof(buf), "%ldB", (long)size);
    else if (size < 1024*1024) snprintf(buf, sizeof(buf), "%.1fK", (double)size / 1024.0);
    else if (size < 1024*1024*1024) snprintf(buf, sizeof(buf), "%.1fM", (double)size / (1024.0*1024.0));
    else snprintf(buf, sizeof(buf), "%.1fG", (double)size / (1024.0*1024.0*1024.0));
    return buf;
}

static void render(vui_ctx_t *ctx)
{
    vui_clear(ctx, VUI_BG);

    /* Title bar */
    vui_fill(ctx, 0, 0, 1, ctx->cols, VUI_SURFACE);
    vui_text_bold(ctx, 0, 2, " VGP Files ", VUI_ACCENT, VUI_SURFACE);

    /* Path bar */
    vui_fill(ctx, 1, 0, 1, ctx->cols, (vui_color_t){0x18, 0x18, 0x28});
    vui_text(ctx, 1, 2, fs.cwd, VUI_WHITE, (vui_color_t){0x18, 0x18, 0x28});

    /* Column headers */
    vui_text_bold(ctx, 3, 2, "Name", VUI_GRAY, VUI_BG);
    vui_text_bold(ctx, 3, ctx->cols - 20, "Size", VUI_GRAY, VUI_BG);
    vui_hline(ctx, 4, 0, ctx->cols, VUI_BORDER, VUI_BG);

    /* File list */
    int list_start = 5;
    int visible = ctx->rows - list_start - 2;

    /* Handle keyboard navigation */
    if (ctx->key_pressed) {
        if (ctx->last_keysym == 0xFF52 && fs.selected > 0) fs.selected--; /* Up */
        if (ctx->last_keysym == 0xFF54 && fs.selected < fs.entry_count - 1) fs.selected++; /* Down */
        if (ctx->last_keysym == 0xFF0D) { /* Enter */
            if (fs.selected >= 0 && fs.selected < fs.entry_count) {
                file_entry_t *fe = &fs.entries[fs.selected];
                if (fe->is_dir) navigate(fe->name);
            }
        }
        if (ctx->last_keysym == 0xFF1B) ctx->running = false; /* Escape */

        /* Keep selection visible */
        if (fs.selected < fs.scroll) fs.scroll = fs.selected;
        if (fs.selected >= fs.scroll + visible) fs.scroll = fs.selected - visible + 1;
    }

    for (int i = 0; i < visible && fs.scroll + i < fs.entry_count; i++) {
        int idx = fs.scroll + i;
        file_entry_t *fe = &fs.entries[idx];
        int row = list_start + i;
        bool selected = (idx == fs.selected);
        bool hover = (ctx->mouse_row == row);

        vui_color_t bg = selected ? VUI_ACCENT : (hover ? VUI_SURFACE : VUI_BG);
        vui_color_t fg = selected ? VUI_WHITE : VUI_WHITE;
        vui_fill(ctx, row, 0, 1, ctx->cols, bg);

        /* Icon */
        if (fe->is_dir) {
            vui_set_cell(ctx, row, 2, 0x1F4C1, VUI_YELLOW, bg, 0); /* 📁 folder */
            vui_text(ctx, row, 4, fe->name, VUI_ACCENT, bg);
        } else {
            vui_set_cell(ctx, row, 2, 0x1F4C4, VUI_GRAY, bg, 0); /* 📄 file */
            vui_text(ctx, row, 4, fe->name, fg, bg);
        }

        /* Size */
        if (!fe->is_dir) {
            const char *sz = format_size(fe->size);
            vui_text(ctx, row, ctx->cols - 12, sz, VUI_GRAY, bg);
        }

        /* Click to select/open */
        if (hover && ctx->mouse_clicked) {
            if (idx == fs.selected && fe->is_dir) {
                navigate(fe->name); /* double-click to enter dir */
                return;
            }
            fs.selected = idx;
        }
    }

    vui_scrollbar(ctx, list_start, ctx->cols - 1, visible,
                   visible, fs.entry_count, fs.scroll);

    /* Status bar */
    vui_fill(ctx, ctx->rows - 1, 0, 1, ctx->cols, VUI_SURFACE);
    char status[128];
    snprintf(status, sizeof(status), " %d items | Arrows: navigate | Enter: open | Esc: quit",
             fs.entry_count);
    vui_text(ctx, ctx->rows - 1, 1, status, VUI_GRAY, VUI_SURFACE);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;

    FILE *logfile = fopen("/tmp/vgp-files.log", "w");
    if (logfile) { setvbuf(logfile, NULL, _IOLBF, 0); dup2(fileno(logfile), STDERR_FILENO); fclose(logfile); }

    /* Start in home directory */
    const char *home = getenv("HOME");
    snprintf(fs.cwd, sizeof(fs.cwd), "%s", home ? home : "/");
    scan_dir();

    vui_ctx_t ctx;
    if (vui_init(&ctx, "VGP Files", 800, 500) < 0)
        return 1;

    vui_run(&ctx, render);
    vui_destroy(&ctx);
    return 0;
}
