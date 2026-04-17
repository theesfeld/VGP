/* SPDX-License-Identifier: MIT */
#include "pool.h"
#include <stdlib.h>
#include <string.h>

#define ALIGN_UP(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define POOL_ALIGNMENT 16

int vgp_pool_init(vgp_pool_t *pool, size_t obj_size, size_t capacity)
{
    pool->obj_size = ALIGN_UP(obj_size, POOL_ALIGNMENT);
    pool->capacity = capacity;

    pool->base = aligned_alloc(POOL_ALIGNMENT, pool->obj_size * capacity);
    if (!pool->base)
        return -1;
    memset(pool->base, 0, pool->obj_size * capacity);

    pool->free_stack = malloc(capacity * sizeof(uint32_t));
    if (!pool->free_stack) {
        free(pool->base);
        pool->base = NULL;
        return -1;
    }

    /* Push all indices onto the free stack (reverse order so index 0 is allocated first) */
    pool->free_top = (int)capacity - 1;
    for (size_t i = 0; i < capacity; i++) {
        pool->free_stack[i] = (uint32_t)(capacity - 1 - i);
    }

    return 0;
}

void vgp_pool_destroy(vgp_pool_t *pool)
{
    free(pool->base);
    free(pool->free_stack);
    pool->base = NULL;
    pool->free_stack = NULL;
}

void *vgp_pool_alloc(vgp_pool_t *pool)
{
    if (pool->free_top < 0)
        return NULL;

    uint32_t idx = pool->free_stack[pool->free_top--];
    void *ptr = pool->base + idx * pool->obj_size;
    memset(ptr, 0, pool->obj_size);
    return ptr;
}

void vgp_pool_free(vgp_pool_t *pool, void *ptr)
{
    if (!ptr)
        return;

    uint32_t idx = vgp_pool_index_of(pool, ptr);
    if (idx >= pool->capacity)
        return;

    pool->free_stack[++pool->free_top] = idx;
}

void *vgp_pool_get(vgp_pool_t *pool, uint32_t index)
{
    if (index >= pool->capacity)
        return NULL;
    return pool->base + index * pool->obj_size;
}

uint32_t vgp_pool_index_of(vgp_pool_t *pool, void *ptr)
{
    ptrdiff_t diff = (uint8_t *)ptr - pool->base;
    if (diff < 0)
        return (uint32_t)pool->capacity;
    return (uint32_t)(diff / (ptrdiff_t)pool->obj_size);
}