#ifndef __RAXEL_VOXEL_H__
#define __RAXEL_VOXEL_H__

#include <cglm/cglm.h>
#include <raxel/core/util.h>  // for raxel_allocator_t, raxel_string_t, etc.
#include <stdint.h>

typedef uint8_t raxel_material_handle_t;
typedef int32_t raxel_chunk_coord_t;

typedef struct raxel_voxel {
    raxel_material_handle_t material;
} raxel_voxel_t;

#define RAXEL_VOXEL_CHUNK_SIZE 32

typedef struct raxel_voxel_chunk {
    raxel_chunk_coord_t x, y;
    raxel_voxel_t voxels[RAXEL_VOXEL_CHUNK_SIZE][RAXEL_VOXEL_CHUNK_SIZE][RAXEL_VOXEL_CHUNK_SIZE];
} raxel_voxel_chunk_t;

typedef struct raxel_voxel_material {
    raxel_string_t name;
    vec4 color;
} raxel_voxel_material_t;

typedef struct raxel_voxel_world {
    raxel_allocator_t world_allocator;
    raxel_list(raxel_voxel_chunk_t *) loaded_chunks;
    raxel_list(raxel_voxel_material_t) materials;
} raxel_voxel_world_t;

// The internal context for our voxel-world allocator.
typedef struct __raxel_voxel_world_allocator_ctx {
    raxel_size_t max_chunks;
    void *buffer;  // A contiguous block for max_chunks * sizeof(raxel_voxel_chunk_t)
    raxel_list(raxel_size_t) free_chunks;
    raxel_list(raxel_size_t) used_chunks;
    raxel_allocator_t list_allocator;
} __raxel_voxel_world_allocator_ctx_t;

/**
 * Creates a voxel world allocator that preallocates enough space for max_chunks
 * voxel chunks and tracks which chunks are loaded.
 *
 * @param max_chunks The maximum number of voxel chunks.
 * @return A raxel_allocator_t that will allocate memory from the preallocated buffer.
 */
raxel_allocator_t raxel_voxel_world_allocator(raxel_size_t max_chunks);

/**
 * Destroy the voxel world allocator created by raxel_voxel_world_allocator.
 *
 * @param allocator Pointer to the allocator to destroy.
 */
void raxel_voxel_world_allocator_destroy(raxel_allocator_t *allocator);

#endif  // __RAXEL_VOXEL_H__
