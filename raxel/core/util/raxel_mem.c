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

static void *raxel_arena_alloc(void *ctx, raxel_size_t size) {
    raxel_arena_ctx_t *arena_ctx = (raxel_arena_ctx_t *)ctx;
    if (arena_ctx->used + size > arena_ctx->size) {
        return NULL;
    }
    void *ptr = (void *)((char *)arena_ctx->arena + arena_ctx->used);
    arena_ctx->used += size;
    return ptr;
}

static void raxel_arena_free(void *ctx, void *ptr) {
    // Do nothing
}

static void *raxel_arena_copy(void *dest, const void *src, raxel_size_t n) {
    return memcpy(dest, src, n);
}

raxel_allocator_t *raxel_arena_allocator(raxel_size_t size) {
    raxel_arena_ctx_t *arena_ctx = malloc(sizeof(raxel_arena_ctx_t));
    arena_ctx->arena = malloc(size);
    arena_ctx->size = size;
    arena_ctx->used = 0;
    return &(raxel_allocator_t){
        .ctx = arena_ctx,
        .alloc = raxel_arena_alloc,
        .free = raxel_arena_free,
        .copy = raxel_arena_copy};
}

void raxel_arena_allocator_destroy(raxel_allocator_t *allocator) {
    raxel_arena_ctx_t *arena_ctx = (raxel_arena_ctx_t *)allocator->ctx;
    free(arena_ctx->arena);
    free(arena_ctx);
    free(allocator);
}