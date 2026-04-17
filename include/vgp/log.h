/* SPDX-License-Identifier: MIT */
#ifndef VGP_LOG_H
#define VGP_LOG_H

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

typedef enum {
    VGP_LOG_LEVEL_ERROR = 0,
    VGP_LOG_LEVEL_WARN  = 1,
    VGP_LOG_LEVEL_INFO  = 2,
    VGP_LOG_LEVEL_DEBUG = 3,
} vgp_log_level_t;

/* Global log level -- set at startup */
extern vgp_log_level_t vgp_log_level;

#define VGP_LOG(level, tag, fmt, ...) \
    do { \
        if ((level) <= vgp_log_level) { \
            struct timespec _ts; \
            clock_gettime(CLOCK_MONOTONIC, &_ts); \
            const char *_names[] = { "ERROR", "WARN", "INFO", "DEBUG" }; \
            fprintf(stderr, "[%5ld.%03ld] %-5s [%s] " fmt "\n", \
                    (long)_ts.tv_sec, _ts.tv_nsec / 1000000, \
                    _names[(level)], (tag), ##__VA_ARGS__); \
        } \
    } while (0)

#define VGP_LOG_ERROR(tag, fmt, ...) VGP_LOG(VGP_LOG_LEVEL_ERROR, tag, fmt, ##__VA_ARGS__)
#define VGP_LOG_WARN(tag, fmt, ...)  VGP_LOG(VGP_LOG_LEVEL_WARN,  tag, fmt, ##__VA_ARGS__)
#define VGP_LOG_INFO(tag, fmt, ...)  VGP_LOG(VGP_LOG_LEVEL_INFO,  tag, fmt, ##__VA_ARGS__)

#ifdef VGP_DEBUG
#define VGP_LOG_DEBUG(tag, fmt, ...) VGP_LOG(VGP_LOG_LEVEL_DEBUG, tag, fmt, ##__VA_ARGS__)
#else
#define VGP_LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

/* Log with errno context */
#define VGP_LOG_ERRNO(tag, fmt, ...) \
    VGP_LOG_ERROR(tag, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#endif /* VGP_LOG_H */