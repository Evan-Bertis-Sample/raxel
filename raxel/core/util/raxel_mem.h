#ifndef __RAXEL_MEM_H__
#define __RAXEL_MEM_H__

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#include <stdint.h>
#include <stdlib.h>

typedef size_t raxel_size_t;

typedef struct {
    void *ctx;
    void *(*alloc)(void *ctx, size_t size);
    void (*free)(void *ctx, void *ptr);
} raxel_allocator_t;

void *raxel_malloc(raxel_allocator_t *allocator, raxel_size_t size);

void raxel_free(raxel_allocator_t *allocator, void *ptr);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif // __RAXEL_MEM_H__