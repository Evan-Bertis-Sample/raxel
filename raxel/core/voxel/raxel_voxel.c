// raxel_voxel.c

#include "raxel_voxel.h"

#include <math.h>
#include <raxel/core/graphics/passes/compute_pass.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Voxel World Creation / Destruction
// -----------------------------------------------------------------------------

raxel_voxel_world_t *raxel_voxel_world_create(raxel_allocator_t *allocator) {
    raxel_voxel_world_t *world = raxel_malloc(allocator, sizeof(raxel_voxel_world_t));
    world->allocator = allocator;
    // Create dynamic lists for chunks, chunk meta, and materials.
    world->chunks = raxel_list_create_reserve(raxel_voxel_chunk_t, allocator, RAXEL_MAX_LOADED_CHUNKS);

    world->chunk_meta = raxel_list_create_reserve(raxel_voxel_chunk_meta_t, allocator, RAXEL_MAX_LOADED_CHUNKS);
    world->__num_loaded_chunks = 0;
    world->materials = raxel_list_create_reserve(raxel_voxel_material_t, allocator, 16);
    return world;
}

void raxel_voxel_world_destroy(raxel_voxel_world_t *world) {
    // Destroy each material’s string.
    for (raxel_size_t i = 0; i < raxel_list_size(world->materials); i++) {
        raxel_voxel_material_t *mat = &world->materials[i];
        raxel_string_destroy(&mat->name);
    }
    raxel_list_destroy(world->materials);
    raxel_list_destroy(world->chunk_meta);
    raxel_list_destroy(world->chunks);
    raxel_free(world->allocator, world);
}

// -----------------------------------------------------------------------------
// Materials
// -----------------------------------------------------------------------------

void raxel_voxel_world_add_material(raxel_voxel_world_t *world, raxel_string_t name, raxel_voxel_material_attributes_t attributes) {
    raxel_voxel_material_t material = {
        .name = name,
        .attributes = attributes,
    };
    raxel_list_push_back(world->materials, material);
}

raxel_material_handle_t raxel_voxel_world_get_material_handle(raxel_voxel_world_t *world, raxel_string_t name) {
    for (raxel_size_t i = 0; i < raxel_list_size(world->materials); i++) {
        if (raxel_string_compare(&world->materials[i].name, &name) == 0) {
            return (raxel_material_handle_t)i;
        }
    }
    // If not found, you might return a default handle or signal an error.
    return 0;
}

// -----------------------------------------------------------------------------
// Chunk Access
// -----------------------------------------------------------------------------

static raxel_voxel_chunk_t *__raxel_voxel_world_create_chunk(raxel_voxel_world_t *world,
                                             raxel_coord_t x,
                                             raxel_coord_t y,
                                             raxel_coord_t z) {
    raxel_voxel_chunk_meta_t meta = {x, y, z, RAXEL_VOXEL_CHUNK_STATE_COUNT};
    RAXEL_CORE_LOG("Pushing back chunk meta\n");
    raxel_list_push_back(world->chunk_meta, meta);
    raxel_voxel_chunk_t chunk;
    memset(&chunk, 0, sizeof(raxel_voxel_chunk_t));
    RAXEL_CORE_LOG("Pushing back chunk\n");
    raxel_list_push_back(world->chunks, chunk);
    RAXEL_CORE_LOG("New chunk created at (%d, %d, %d), index %d\n", x, y, z, raxel_list_size(world->chunks) - 1);
    return &world->chunks[raxel_list_size(world->chunks) - 1];
}

static void __raxel_voxel_world_swap_chunks(raxel_voxel_world_t *world,
                                            raxel_size_t i,
                                            raxel_size_t j) {
    raxel_voxel_chunk_meta_t tmp_meta = world->chunk_meta[i];
    world->chunk_meta[i] = world->chunk_meta[j];
    world->chunk_meta[j] = tmp_meta;
    raxel_voxel_chunk_t tmp_chunk = world->chunks[i];
    world->chunks[i] = world->chunks[j];
    world->chunks[j] = tmp_chunk;
}

// Returns a pointer to a chunk given its chunk coordinates (relative to chunk grid).
raxel_voxel_chunk_t *raxel_voxel_world_get_chunk(raxel_voxel_world_t *world,
                                                 raxel_coord_t x,
                                                 raxel_coord_t y,
                                                 raxel_coord_t z) {
    // scan through though all chunks in the list
    for (raxel_size_t i = 0; i < raxel_list_size(world->chunk_meta); i++) {
        raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
        if (meta->x == x && meta->y == y && meta->z == z) {
            return &world->chunks[i];
        }
    }
    return NULL;
}

// Given world coordinates, returns the voxel stored at that location.
// This function computes which chunk the voxel lies in and then the voxel’s
// local coordinates within that chunk.
raxel_voxel_t raxel_voxel_world_get_voxel(raxel_voxel_world_t *world,
                                          raxel_coord_t x,
                                          raxel_coord_t y,
                                          raxel_coord_t z) {
    raxel_coord_t chunk_x = x / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t chunk_y = y / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t chunk_z = z / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_voxel_chunk_t *chunk = raxel_voxel_world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        raxel_voxel_t empty = {0};
        return empty;
    }
    raxel_coord_t local_x = x % RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t local_y = y % RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t local_z = z % RAXEL_VOXEL_CHUNK_SIZE;

    raxel_size_t index = local_x + local_y * RAXEL_VOXEL_CHUNK_SIZE + local_z * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE;
    return chunk->voxels[index];
}

// Places a voxel at the given world coordinates by updating the appropriate chunk.
// If the chunk is not loaded, nothing is done (alternatively, you could choose to load it).
void raxel_voxel_world_place_voxel(raxel_voxel_world_t *world,
                                   raxel_coord_t x,
                                   raxel_coord_t y,
                                   raxel_coord_t z,
                                   raxel_voxel_t voxel) {
    raxel_coord_t chunk_x = x / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t chunk_y = y / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t chunk_z = z / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_voxel_chunk_t *chunk = raxel_voxel_world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        // we'll need to create a new chunk
        RAXEL_CORE_LOG("Creating new chunk at (%d, %d, %d)\n", chunk_x, chunk_y, chunk_z);
        chunk = __raxel_voxel_world_create_chunk(world, chunk_x, chunk_y, chunk_z);
        if (!chunk) {
            RAXEL_CORE_FATAL_ERROR("Failed to create chunk at (%d, %d, %d)\n", chunk_x, chunk_y, chunk_z);
        }
    }

    raxel_coord_t local_x = x % RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t local_y = y % RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t local_z = z % RAXEL_VOXEL_CHUNK_SIZE;

    raxel_size_t index = local_x + local_y * RAXEL_VOXEL_CHUNK_SIZE + local_z * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE;
    chunk->voxels[index] = voxel;

}

// -----------------------------------------------------------------------------
// Voxel World Update (Chunk Loading / Unloading)
// -----------------------------------------------------------------------------

// Updates the voxel world by unloading chunks that are too far from the camera and
// loading new chunks within the view distance. The loaded chunks (and their meta)
// remain contiguous in the arrays (i.e. indices [0, __num_loaded_chunks-1]).
void raxel_voxel_world_update(raxel_voxel_world_t *world,
                              raxel_voxel_world_update_options_t *options) {
    // Determine the camera's chunk coordinates.
    RAXEL_CORE_LOG("Updating voxel world\n");
    raxel_coord_t cam_chunk_x = options->camera_position[0] / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t cam_chunk_y = options->camera_position[1] / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t cam_chunk_z = options->camera_position[2] / RAXEL_VOXEL_CHUNK_SIZE;

    // find the number chunks that are actually within the view distance
    raxel_size_t num_chunks = raxel_list_size(world->chunk_meta);
    raxel_size_t num_loaded_chunks = 0;
    // swap the chunks that are within the view distance to the front of the list
    for (raxel_size_t i = 0; i < num_chunks; i++) {
        raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
        raxel_coord_t dx = meta->x - cam_chunk_x;
        raxel_coord_t dy = meta->y - cam_chunk_y;
        raxel_coord_t dz = meta->z - cam_chunk_z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        if (dist < options->view_distance) {
            if (num_loaded_chunks != i) {
                __raxel_voxel_world_swap_chunks(world, num_loaded_chunks, i);
            }
            num_loaded_chunks++;
        }
    }
    world->__num_loaded_chunks = num_loaded_chunks;

    // print out some information about the voxel world
    printf("Loaded chunks: %d\n", world->__num_loaded_chunks);
    for (raxel_size_t i = 0; i < world->__num_loaded_chunks; i++) {
        raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
        printf("Chunk %d: (%d, %d, %d)\n", i, meta->x, meta->y, meta->z);

        // count the number of non-air voxels in this chunk
        int count = 0;
        raxel_voxel_chunk_t *chunk = &world->chunks[i];

        for (raxel_size_t j = 0; j < RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE; j++) {
            if (chunk->voxels[j].material != 0) {
                count++;
            }
        }

        printf("  Non-air voxels: %d\n", count);
    }
}

void raxel_voxel_world_set_sb(raxel_voxel_world_t *world, raxel_compute_shader_t *compute_shader, raxel_pipeline_t *pipeline) {
    // Set up the storage buffer for the voxel world data.
    // The size of our data is the size of GPUVoxelWorld.
    raxel_sb_buffer_desc_t sb_desc = RAXEL_SB_DESC(
        (raxel_sb_entry_t){.name = "voxel_world", .offset = 0, .size = sizeof(__raxel_voxel_world_gpu_t)});
    // This call will create the buffer and update binding 1 of the compute shader’s descriptor set.
    raxel_compute_shader_set_sb(compute_shader, pipeline, &sb_desc);
}

void raxel_voxel_world_dispatch_sb(raxel_voxel_world_t *world, raxel_compute_shader_t *compute_shader, raxel_pipeline_t *pipeline) {
    // Copy the voxel world data into the storage buffer.
    __raxel_voxel_world_gpu_t gpu_world = {0};
    gpu_world.num_loaded_chunks = (uint32_t)world->__num_loaded_chunks;
    for (uint32_t i = 0; i < gpu_world.num_loaded_chunks; i++) {
        gpu_world.chunk_meta[i] = world->chunk_meta[i];
        gpu_world.chunks[i] = world->chunks[i];
    }

    RAXEL_CORE_LOG("Dispatching voxel world with %d chunks\n", gpu_world.num_loaded_chunks);

    // Update the storage buffer with the new voxel world data.
    memcpy(compute_shader->sb_buffer->data, &gpu_world, sizeof(__raxel_voxel_world_gpu_t));
    raxel_sb_buffer_update(compute_shader->sb_buffer, pipeline);
}