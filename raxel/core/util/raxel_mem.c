#include "raxel_mem.h"

#include <stdlib.h>
#include <string.h>

void *raxel_malloc(raxel_allocator_t *allocator, raxel_size_t size) {
    return allocator->alloc(allocator->ctx, size);
}

void raxel_free(raxel_allocator_t *allocator, void *ptr) {
    allocator->free(allocator->ctx, ptr);
}

void *raxel_copy(raxel_allocator_t *allocator, void *dest, const void *src, raxel_size_t n) {
    return allocator->copy(dest, src, n);
}

raxel_allocator_t raxel_default_allocator() {
    return (raxel_allocator_t){
        .ctx = NULL,
        .alloc = malloc,
        .free = free,
        .copy = memcpy};
}