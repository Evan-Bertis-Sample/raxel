#include "raxel_mem.h"

void *raxel_malloc(raxel_allocator_t *allocator, raxel_size_t size) {
    return allocator->alloc(allocator->ctx, size);
}

void raxel_free(raxel_allocator_t *allocator, void *ptr) {
    allocator->free(allocator->ctx, ptr);
}