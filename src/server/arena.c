#include "arena.h"
#include <stdlib.h>
#include <string.h>

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define ARENA_ALIGNMENT 16

int vgp_arena_init(vgp_arena_t *arena, size_t capacity)
{
    arena->base = aligned_alloc(ARENA_ALIGNMENT, capacity);
    if (!arena->base)
        return -1;
    arena->capacity = capacity;
    arena->offset = 0;
    return 0;
}

void vgp_arena_destroy(vgp_arena_t *arena)
{
    free(arena->base);
    arena->base = NULL;
    arena->capacity = 0;
    arena->offset = 0;
}

void *vgp_arena_alloc(vgp_arena_t *arena, size_t size)
{
    size_t aligned = ALIGN_UP(size, ARENA_ALIGNMENT);
    if (arena->offset + aligned > arena->capacity)
        return NULL;
    void *ptr = arena->base + arena->offset;
    arena->offset += aligned;
    return ptr;
}

void vgp_arena_reset(vgp_arena_t *arena)
{
    arena->offset = 0;
}

size_t vgp_arena_save(const vgp_arena_t *arena)
{
    return arena->offset;
}

void vgp_arena_restore(vgp_arena_t *arena, size_t savepoint)
{
    if (savepoint <= arena->capacity)
        arena->offset = savepoint;
}
