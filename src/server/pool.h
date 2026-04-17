/* SPDX-License-Identifier: MIT */
#ifndef VGP_POOL_H
#define VGP_POOL_H

#include <stddef.h>
#include <stdint.h>

typedef struct vgp_pool {
    uint8_t  *base;
    size_t    obj_size;
    size_t    capacity;
    uint32_t *free_stack;
    int       free_top;
} vgp_pool_t;

int    vgp_pool_init(vgp_pool_t *pool, size_t obj_size, size_t capacity);
void   vgp_pool_destroy(vgp_pool_t *pool);
void  *vgp_pool_alloc(vgp_pool_t *pool);
void   vgp_pool_free(vgp_pool_t *pool, void *ptr);
void  *vgp_pool_get(vgp_pool_t *pool, uint32_t index);
uint32_t vgp_pool_index_of(vgp_pool_t *pool, void *ptr);

#endif /* VGP_POOL_H */