#ifndef VGP_XDG_H
#define VGP_XDG_H

/* XDG Base Directory Specification helpers.
 *
 * Spec: https://specifications.freedesktop.org/basedir-spec/latest/
 *
 * Resolution order (all "_home" helpers fall back to the spec default
 * under $HOME when the env var is unset or empty):
 *
 *   XDG_CONFIG_HOME  -> $HOME/.config
 *   XDG_DATA_HOME    -> $HOME/.local/share
 *   XDG_STATE_HOME   -> $HOME/.local/state
 *   XDG_CACHE_HOME   -> $HOME/.cache
 *   XDG_RUNTIME_DIR  -> (required; no fallback per spec -- we fall back
 *                        to /tmp/vgp-<uid> so sockets work in CI).
 *   XDG_CONFIG_DIRS  -> /etc/xdg          (colon-separated)
 *   XDG_DATA_DIRS    -> /usr/local/share:/usr/share
 *
 * All vgp_xdg_resolve_* write an absolute path into buf and return 1
 * on success, 0 on failure (HOME missing). Writers also mkdir -p the
 * parent directory with 0700.
 *
 * vgp_xdg_find_* searches the write location first, then the system
 * search path, and returns the first existing file. Read-only. */

#include <stddef.h>
#include <stdbool.h>

/* Base dir kinds. */
typedef enum {
    VGP_XDG_CONFIG,   /* config files (read/write, per-user) */
    VGP_XDG_DATA,     /* shared data (fonts, shaders, themes) */
    VGP_XDG_STATE,    /* logs, history, session snapshots */
    VGP_XDG_CACHE,    /* regenerable caches */
    VGP_XDG_RUNTIME,  /* sockets, PIDs, runtime-only */
} vgp_xdg_kind_t;

/* Write absolute path "<home_for_kind>/<subpath>" into buf.
 * Creates all parent directories of the returned path with 0700
 * permissions. Returns 1 on success, 0 if HOME isn't set (unless
 * kind == VGP_XDG_RUNTIME, where we fall back to /tmp/vgp-<uid>). */
int vgp_xdg_resolve(vgp_xdg_kind_t kind,
                      const char *subpath,
                      char *buf, size_t buf_sz);

/* Ensure a directory exists (mkdir -p, mode 0700). Returns 1 on
 * success, 0 on failure. */
int vgp_xdg_mkpath(const char *dir);

/* Search for an existing file named "vgp/<subpath>" across:
 *   1. XDG_CONFIG_HOME (or XDG_DATA_HOME for VGP_XDG_DATA)
 *   2. XDG_CONFIG_DIRS (or XDG_DATA_DIRS)
 * Writes the path into buf. Returns 1 if a file was found, 0 otherwise. */
int vgp_xdg_find_config(const char *subpath, char *buf, size_t buf_sz);
int vgp_xdg_find_data(const char *subpath, char *buf, size_t buf_sz);

#endif /* VGP_XDG_H */
