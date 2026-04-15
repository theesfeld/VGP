#ifndef VGP_ARENA_H
#define VGP_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct vgp_arena {
    uint8_t *base;
    size_t   capacity;
    size_t   offset;
} vgp_arena_t;

int    vgp_arena_init(vgp_arena_t *arena, size_t capacity);
void   vgp_arena_destroy(vgp_arena_t *arena);
void  *vgp_arena_alloc(vgp_arena_t *arena, size_t size);
void   vgp_arena_reset(vgp_arena_t *arena);
size_t vgp_arena_save(const vgp_arena_t *arena);
void   vgp_arena_restore(vgp_arena_t *arena, size_t savepoint);

#endif /* VGP_ARENA_H */
