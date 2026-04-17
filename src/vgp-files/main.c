/* VGP Files -- DTE (Data Transfer Equipment) MFD page.
 * Directory listing as columned data rows, path boxed at the top,
 * OSB buttons for UP / HOME / ROOT / REFRESH / OPEN. */

#include "vgp-gfx.h"
#include "vgp-hud.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_FILES 1024

typedef struct {
    char  name[256];
    bool  is_dir;
    off_t size;
} file_entry_t;

static struct {
    char         cwd[1024];
    file_entry_t files[MAX_FILES];
    int          count;
    int          selected;
    int          scroll;
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
    else if (size < 1024L*1024) snprintf(buf, buf_sz, "%.1f KB", (double)size / 1024);
    else if (size < 1024L*1024*1024) snprintf(buf, buf_sz, "%.1f MB", (double)size / (1024*1024));
    else snprintf(buf, buf_sz, "%.1f GB", (double)size / (1024.0*1024*1024));
}

static void render(vgfx_ctx_t *ctx)
{
    hud_palette_t P = hud_palette();
    vgfx_clear(ctx, vgfx_rgba(0, 0, 0, 0));

    /* OSB layout. Top = nav, Bottom = file ops. */
    hud_osb_t top_osb[] = {
        { "UP",      false, true },
        { "HOME",    false, true },
        { "ROOT",    false, true },
        { "REFRESH", false, true },
    };
    hud_osb_t bot_osb[] = {
        { "OPEN",    false, fm.count > 0 },
        { "COPY",    false, false },
        { "DEL",     false, false },
        { "MKDIR",   false, false },
    };

    hud_mfd_t mfd = { 0 };
    mfd.top = top_osb;     mfd.top_count = 4;
    mfd.bottom = bot_osb;  mfd.bottom_count = 4;
    mfd.title = "DTE-FILES";

    float cx, cy, cw, ch;
    hud_mfd_frame(ctx, &mfd, &P, &cx, &cy, &cw, &ch);

    /* Handle OSB clicks */
    if (mfd.clicked_edge == 1) { /* top */
        switch (mfd.clicked_index) {
        case 0: navigate(".."); break;
        case 1: {
            const char *home = getenv("HOME");
            if (home) { snprintf(fm.cwd, sizeof(fm.cwd), "%s", home); scan_dir(); }
        } break;
        case 2: snprintf(fm.cwd, sizeof(fm.cwd), "/"); scan_dir(); break;
        case 3: scan_dir(); break;
        }
    }
    if (mfd.clicked_edge == 3 && mfd.clicked_index == 0 && fm.count > 0) {
        if (fm.files[fm.selected].is_dir) navigate(fm.files[fm.selected].name);
    }

    /* --- Path bar: boxed ETCHED "PATH" + projected cwd --- */
    float fs = 13.0f;
    float ph = 24.0f;
    vgfx_rect_outline(ctx, cx, cy, cw, ph, 1.0f, P.dim);
    hud_etched(ctx, "PATH", cx + 6, cy + ph * 0.5f + fs * 0.35f, fs - 2, &P);
    vgfx_text_bold(ctx, fm.cwd, cx + 44, cy + ph * 0.5f + fs * 0.35f, fs, P.warn);

    float ly = cy + ph + 8.0f;
    float lh = ch - ph - 8.0f;

    /* --- Column headers (ETCHED) --- */
    float hdr_fs = 11.0f;
    hud_etched_bold(ctx, "T",      cx + 6,                 ly + hdr_fs, hdr_fs, &P);
    hud_etched_bold(ctx, "NAME",   cx + 28,                ly + hdr_fs, hdr_fs, &P);
    hud_etched_bold(ctx, "SIZE",   cx + cw - 100,          ly + hdr_fs, hdr_fs, &P);
    vgfx_line(ctx, cx, ly + hdr_fs + 4, cx + cw, ly + hdr_fs + 4, 1.0f, P.dim);

    ly += hdr_fs + 8.0f;
    lh -= hdr_fs + 8.0f;

    /* Keyboard nav */
    if (ctx->key_pressed) {
        if (ctx->last_keysym == 0xFF52 && fm.selected > 0) fm.selected--;
        if (ctx->last_keysym == 0xFF54 && fm.selected < fm.count - 1) fm.selected++;
        if (ctx->last_keysym == 0xFF0D && fm.selected < fm.count) {
            if (fm.files[fm.selected].is_dir) navigate(fm.files[fm.selected].name);
        }
        if (ctx->last_keysym == 0xFF1B) navigate("..");
    }

    /* --- Rows --- */
    float row_h = 20.0f;
    int visible = (int)(lh / row_h);
    if (visible < 1) visible = 1;

    if (fm.selected < fm.scroll) fm.scroll = fm.selected;
    if (fm.selected >= fm.scroll + visible) fm.scroll = fm.selected - visible + 1;

    vgfx_push_clip(ctx, cx, ly, cw, lh);

    for (int i = fm.scroll; i < fm.count && i < fm.scroll + visible; i++) {
        float ry = ly + (float)(i - fm.scroll) * row_h;
        bool hover = (ctx->mouse_y >= ry && ctx->mouse_y < ry + row_h &&
                       ctx->mouse_x >= cx && ctx->mouse_x < cx + cw);
        bool sel = (i == fm.selected);

        /* Type indicator -- etched box */
        const char *tchar = fm.files[i].is_dir ? "D" : "F";
        vgfx_color_t tc = fm.files[i].is_dir ? P.warn : P.fg;
        vgfx_rect_outline(ctx, cx + 4, ry + 2, 16, row_h - 4, 0.8f, P.dim);
        float tw = vgfx_text_width(ctx, tchar, -1, 11);
        vgfx_text_bold(ctx, tchar, cx + 4 + (16 - tw) * 0.5f,
                        ry + row_h * 0.5f + 11 * 0.35f, 11, tc);

        /* Name */
        vgfx_color_t nc = sel ? P.hi : (hover ? P.fg : P.fg);
        vgfx_text(ctx, fm.files[i].name, cx + 28,
                   ry + row_h * 0.5f + fs * 0.35f, fs - 1, nc);

        /* Size (right-aligned) */
        if (!fm.files[i].is_dir) {
            char sz[32]; format_size(fm.files[i].size, sz, sizeof(sz));
            float sw = vgfx_text_width(ctx, sz, -1, fs - 2);
            vgfx_text(ctx, sz, cx + cw - sw - 8,
                       ry + row_h * 0.5f + fs * 0.35f, fs - 2, P.dim);
        }

        if (sel)
            hud_target_box(ctx, cx + 1, ry + 1, cw - 2, row_h - 2, P.warn);

        if (hover && ctx->mouse_clicked) {
            if (i == fm.selected && fm.files[i].is_dir)
                navigate(fm.files[i].name);
            else fm.selected = i;
        }
    }
    vgfx_pop_clip(ctx);

    if (fm.count > visible)
        vgfx_scrollbar(ctx, cx + cw - 6, ly, lh,
                        visible, fm.count, &fm.scroll);

    /* Footer with item count and selection info -- boxed value fields */
    float foot_y = ly + lh + 2.0f;
    char items[32]; snprintf(items, sizeof(items), "%d", fm.count);
    char idx[32];
    if (fm.count > 0) snprintf(idx, sizeof(idx), "%d/%d", fm.selected + 1, fm.count);
    else snprintf(idx, sizeof(idx), "--");

    hud_boxed_field(ctx, cx, foot_y, 110, 12, "ITEMS", items, P.fg, &P);
    hud_boxed_field(ctx, cx + 120, foot_y, 140, 12, "SEL", idx, P.warn, &P);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    const char *home = getenv("HOME");
    snprintf(fm.cwd, sizeof(fm.cwd), "%s", home ? home : "/");
    scan_dir();

    vgfx_ctx_t ctx;
    if (vgfx_init(&ctx, "VGP Files", 780, 540, 0) < 0) return 1;
    vgfx_run(&ctx, render);
    vgfx_destroy(&ctx);
    return 0;
}
