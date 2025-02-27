#include "raxel_voxel.h"

#include <raxel/core/util.h>  // for raxel_allocator_t, raxel_string_t, etc.
#include <stdlib.h>
#include <string.h>

// Custom allocation function for voxel world allocator.
static void *__raxel_voxel_world_alloc(void *ctx, size_t size) {
    __raxel_voxel_world_allocator_ctx_t *alloc_ctx = (__raxel_voxel_world_allocator_ctx_t *)ctx;
    // We expect size to be <= sizeof(raxel_voxel_chunk_t)
    if (size > sizeof(raxel_voxel_chunk_t)) {
        RAXEL_CORE_LOG_ERROR("Invalid size for voxel world allocator\n");
        return NULL;
    }
    if (raxel_list_size(alloc_ctx->free_chunks) == 0) {
        RAXEL_CORE_LOG_ERROR("No free chunks available in voxel world allocator\n");
        return NULL;
    }
    // Pop the last free chunk index.
    raxel_size_t index = *((raxel_size_t *)raxel_list_get(alloc_ctx->free_chunks, raxel_list_size(alloc_ctx->free_chunks) - 1));
    raxel_list_pop_back(alloc_ctx->free_chunks);
    // Optionally, add the index to used_chunks.
    raxel_list_push_back(alloc_ctx->used_chunks, index);
    // Return the pointer to the chunk.
    return (char *)alloc_ctx->buffer + index * sizeof(raxel_voxel_chunk_t);
}

// Custom free function for voxel world allocator.
static void __raxel_voxel_world_free(void *ctx, void *ptr) {
    __raxel_voxel_world_allocator_ctx_t *alloc_ctx = (__raxel_voxel_world_allocator_ctx_t *)ctx;
    size_t offset = (char *)ptr - (char *)alloc_ctx->buffer;
    raxel_size_t index = offset / sizeof(raxel_voxel_chunk_t);
    // Optionally, remove index from used_chunks (omitted for brevity)
    raxel_list_push_back(alloc_ctx->free_chunks, index);
}

// Custom copy function (simple memcpy).
static void *__raxel_voxel_world_copy(void *dest, const void *src, size_t n) {
    return memcpy(dest, src, n);
}

// Public function to create a voxel world allocator.
raxel_allocator_t raxel_voxel_world_allocator(raxel_size_t max_chunks) {
    __raxel_voxel_world_allocator_ctx_t *ctx = malloc(sizeof(__raxel_voxel_world_allocator_ctx_t));
    if (!ctx) {
    }
    ctx->max_chunks = max_chunks;
    ctx->buffer = malloc(max_chunks * sizeof(raxel_voxel_chunk_t));
    if (!ctx->buffer) {
        RAXEL_CORE_LOG_ERROR("Failed to allocate buffer for voxel world allocator\n");
        free(ctx);
        exit(EXIT_FAILURE);
    }

    ctx->list_allocator = raxel_default_allocator();
    // Create free_chunks list with initial capacity max_chunks.
    ctx->free_chunks = raxel_list_create_reserve(raxel_size_t, &ctx->list_allocator max_chunks);
    ctx->used_chunks = raxel_list_create_reserve(raxel_size_t, &ctx->list_allocator, 0);
    // Initially, all chunk indices are free.
    for (raxel_size_t i = 0; i < max_chunks; i++) {
        raxel_list_push_back(ctx->free_chunks, i);
    }

    raxel_allocator_t allocator;
    allocator.ctx = ctx;
    allocator.alloc = __raxel_voxel_world_alloc;
    allocator.free = __raxel_voxel_world_free;
    allocator.copy = __raxel_voxel_world_copy;
    return allocator;
}

// Public function to destroy a voxel world allocator.
void raxel_voxel_world_allocator_destroy(raxel_allocator_t *allocator) {
    if (!allocator || !allocator->ctx) return;
    __raxel_voxel_world_allocator_ctx_t *ctx = (__raxel_voxel_world_allocator_ctx_t *)allocator->ctx;
    raxel_list_destroy(raxel_default_allocator(), ctx->free_chunks);
    raxel_list_destroy(raxel_default_allocator(), ctx->used_chunks);
    free(ctx->buffer);
    free(ctx);
    allocator->ctx = NULL;
}
