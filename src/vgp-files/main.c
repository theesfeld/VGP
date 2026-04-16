/* VGP Files -- GPU-rendered graphical file manager */

#include "vgp-gfx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES 1024

typedef struct {
    char name[256];
    bool is_dir;
    off_t size;
} file_entry_t;

static struct {
    char cwd[1024];
    file_entry_t files[MAX_FILES];
    int count;
    int selected;
    int scroll;
} fm;

static int cmp_files(const void *a, const void *b)
{
    const file_entry_t *fa = a, *fb = b;
    if (fa->is_dir != fb->is_dir) return fa->is_dir ? -1 : 1;
    return strcasecmp(fa->name, fb->name);
}

static void scan_dir(void)
{
    fm.count = 0;
    fm.selected = 0;
    fm.scroll = 0;
    DIR *dir = opendir(fm.cwd);
    if (!dir) return;
    struct dirent *e;
    while ((e = readdir(dir)) != NULL && fm.count < MAX_FILES) {
        if (e->d_name[0] == '.' && e->d_name[1] == '\0') continue;
        file_entry_t *f = &fm.files[fm.count];
        snprintf(f->name, sizeof(f->name), "%s", e->d_name);
        f->is_dir = (e->d_type == DT_DIR);
        f->size = 0;
        if (!f->is_dir) {
            char path[1280];
            snprintf(path, sizeof(path), "%s/%s", fm.cwd, f->name);
            struct stat st;
            if (stat(path, &st) == 0) f->size = st.st_size;
        }
        fm.count++;
    }
    closedir(dir);
    qsort(fm.files, (size_t)fm.count, sizeof(file_entry_t), cmp_files);
}

static void navigate(const char *name)
{
    if (strcmp(name, "..") == 0) {
        char *slash = strrchr(fm.cwd, '/');
        if (slash && slash != fm.cwd) *slash = '\0';
        else fm.cwd[1] = '\0';
    } else {
        size_t len = strlen(fm.cwd);
        if (len > 1) snprintf(fm.cwd + len, sizeof(fm.cwd) - len, "/%s", name);
        else snprintf(fm.cwd + len, sizeof(fm.cwd) - len, "%s", name);
    }
    scan_dir();
}

static void format_size(off_t size, char *buf, int buf_sz)
{
    if (size < 1024) snprintf(buf, buf_sz, "%ld B", (long)size);
    else if (size < 1024*1024) snprintf(buf, buf_sz, "%.1f KB", (double)size / 1024);
    else if (size < 1024*1024*1024) snprintf(buf, buf_sz, "%.1f MB", (double)size / (1024*1024));
    else snprintf(buf, buf_sz, "%.1f GB", (double)size / (1024*1024*1024));
}

static void render(vgfx_ctx_t *ctx)
{
    vgfx_clear(ctx, vgfx_theme_color(ctx, VGP_THEME_BG));
    float p = ctx->theme.padding;
    float fs = ctx->theme.font_size;
    float w = ctx->width, h = ctx->height;

    /* Path bar */
    vgfx_rounded_rect(ctx, p, p, w - p*2, 30, 4, vgfx_theme_color(ctx, VGP_THEME_BG_SECONDARY));
    vgfx_text(ctx, fm.cwd, p + 10, p + 20, fs, vgfx_theme_color(ctx, VGP_THEME_ACCENT));

    /* Column headers */
    float list_y = p + 40;
    vgfx_text_bold(ctx, "Name", p + 30, list_y + fs, fs - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    vgfx_text_bold(ctx, "Size", w - 120, list_y + fs, fs - 1, vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
    list_y += fs + 8;
    vgfx_separator(ctx, p, list_y, w - p*2);
    list_y += 4;

    /* File list */
    float row_h = fs + 10;
    int visible = (int)((h - list_y - p) / row_h);
    if (visible < 1) visible = 1;

    /* Keyboard nav */
    if (ctx->key_pressed) {
        if (ctx->last_keysym == 0xFF52 && fm.selected > 0) fm.selected--; /* Up */
        if (ctx->last_keysym == 0xFF54 && fm.selected < fm.count - 1) fm.selected++; /* Down */
        if (ctx->last_keysym == 0xFF0D && fm.selected < fm.count) { /* Enter */
            if (fm.files[fm.selected].is_dir) navigate(fm.files[fm.selected].name);
        }
        if (ctx->last_keysym == 0xFF1B) { navigate(".."); } /* Escape = go up */
    }

    /* Scroll to keep selected visible */
    if (fm.selected < fm.scroll) fm.scroll = fm.selected;
    if (fm.selected >= fm.scroll + visible) fm.scroll = fm.selected - visible + 1;

    vgfx_push_clip(ctx, 0, list_y, w, h - list_y - p);
    for (int i = fm.scroll; i < fm.count && i < fm.scroll + visible; i++) {
        float ry = list_y + (float)(i - fm.scroll) * row_h;
        bool hover = (ctx->mouse_y >= ry && ctx->mouse_y < ry + row_h &&
                        ctx->mouse_x >= p && ctx->mouse_x < w - p);
        bool sel = (i == fm.selected);

        if (sel) {
            vgfx_rounded_rect(ctx, p, ry, w - p*2, row_h, 4,
                                vgfx_theme_color(ctx, VGP_THEME_ACCENT));
        } else if (hover) {
            vgfx_rounded_rect(ctx, p, ry, w - p*2, row_h, 4,
                                vgfx_theme_color(ctx, VGP_THEME_BG_TERTIARY));
        }

        if (hover && ctx->mouse_clicked) {
            if (i == fm.selected && fm.files[i].is_dir) navigate(fm.files[i].name);
            else fm.selected = i;
        }

        /* Icon */
        const char *icon = fm.files[i].is_dir ? "D" : "F";
        vgfx_color_t icon_c = fm.files[i].is_dir ?
            vgfx_theme_color(ctx, VGP_THEME_WARNING) : vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY);
        vgfx_text_bold(ctx, icon, p + 10, ry + row_h * 0.5f + fs * 0.35f, fs, icon_c);

        /* Name */
        vgfx_color_t name_c = sel ? vgfx_rgb(1,1,1) : vgfx_theme_color(ctx, VGP_THEME_FG);
        vgfx_text(ctx, fm.files[i].name, p + 30, ry + row_h * 0.5f + fs * 0.35f, fs, name_c);

        /* Size */
        if (!fm.files[i].is_dir) {
            char sz[32]; format_size(fm.files[i].size, sz, sizeof(sz));
            vgfx_text(ctx, sz, w - 120, ry + row_h * 0.5f + fs * 0.35f, fs - 1,
                        sel ? vgfx_rgb(1,1,1) : vgfx_theme_color(ctx, VGP_THEME_FG_SECONDARY));
        }
    }
    vgfx_pop_clip(ctx);

    /* Scrollbar */
    if (fm.count > visible)
        vgfx_scrollbar(ctx, w - p - 8, list_y, h - list_y - p, visible, fm.count, &fm.scroll);

    /* Status */
    char status[64]; snprintf(status, sizeof(status), "%d items", fm.count);
    vgfx_text(ctx, status, p, h - p - 2, fs - 2, vgfx_theme_color(ctx, VGP_THEME_FG_DISABLED));
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    const char *home = getenv("HOME");
    snprintf(fm.cwd, sizeof(fm.cwd), "%s", home ? home : "/");
    scan_dir();

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Files", 750, 500, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
