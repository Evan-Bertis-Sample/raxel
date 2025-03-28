#ifndef __RAXEL_MEM_H__
#define __RAXEL_MEM_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdint.h>
#include <stdlib.h>

typedef size_t raxel_size_t;

typedef struct raxel_allocator_t {
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void (*free)(void *ctx, void *ptr);
    void *(*copy)(void *dest, const void *src, size_t n);
} raxel_allocator_t;

void *raxel_malloc(raxel_allocator_t *allocator, raxel_size_t size);
void raxel_free(raxel_allocator_t *allocator, void *ptr);
void *raxel_copy(raxel_allocator_t *allocator, void *dest, const void *src, raxel_size_t n);

raxel_allocator_t raxel_default_allocator();

typedef struct raxel_arena_ctx {
    void *arena;
    raxel_size_t size;
    raxel_size_t used;
} raxel_arena_ctx_t;

raxel_allocator_t *raxel_arena_allocator(raxel_size_t size);
void raxel_arena_allocator_destroy(raxel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif // __RAXEL_MEM_H__