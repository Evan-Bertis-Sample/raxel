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
    world->prev_update_options = (raxel_voxel_world_update_options_t){0};
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
    // RAXEL_CORE_LOG("Pushing back chunk meta\n");
    raxel_list_push_back(world->chunk_meta, meta);
    raxel_voxel_chunk_t chunk;
    // RAXEL_CORE_LOG("Pushing back chunk\n");
    raxel_list_push_back(world->chunks, chunk);

    // zero out the chunk
    raxel_voxel_chunk_t *new_chunk = &world->chunks[raxel_list_size(world->chunks) - 1];
    memset(new_chunk, 0, sizeof(raxel_voxel_chunk_t));

    // RAXEL_CORE_LOG("New chunk created at (%d, %d, %d), index %d\n", x, y, z, raxel_list_size(world->chunks) - 1);
    return new_chunk;
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

static void __raxel_voxel_world_from_world_to_chunk_coords(raxel_voxel_world_t *world,
                                                           raxel_coord_t x,
                                                           raxel_coord_t y,
                                                           raxel_coord_t z,
                                                           raxel_coord_t *chunk_x,
                                                           raxel_coord_t *chunk_y,
                                                           raxel_coord_t *chunk_z) {
    // For negative numbers, floor division is different from integer division.
    *chunk_x = (x >= 0) ? (x / RAXEL_VOXEL_CHUNK_SIZE)
                        : ((x - RAXEL_VOXEL_CHUNK_SIZE + 1) / RAXEL_VOXEL_CHUNK_SIZE);
    *chunk_y = (y >= 0) ? (y / RAXEL_VOXEL_CHUNK_SIZE)
                        : ((y - RAXEL_VOXEL_CHUNK_SIZE + 1) / RAXEL_VOXEL_CHUNK_SIZE);
    *chunk_z = (z >= 0) ? (z / RAXEL_VOXEL_CHUNK_SIZE)
                        : ((z - RAXEL_VOXEL_CHUNK_SIZE + 1) / RAXEL_VOXEL_CHUNK_SIZE);
}

static raxel_size_t __raxel_voxel_world_from_world_to_index(raxel_voxel_world_t *world,
                                                            raxel_coord_t x,
                                                            raxel_coord_t y,
                                                            raxel_coord_t z) {
    raxel_coord_t chunk_x, chunk_y, chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world, x, y, z, &chunk_x, &chunk_y, &chunk_z);
    raxel_voxel_chunk_t *chunk = raxel_voxel_world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        return 0;
    }
    // Compute local coordinates as: local = world coordinate - (chunk coordinate * chunk_size)
    raxel_coord_t local_x = x - (chunk_x * RAXEL_VOXEL_CHUNK_SIZE);
    raxel_coord_t local_y = y - (chunk_y * RAXEL_VOXEL_CHUNK_SIZE);
    raxel_coord_t local_z = z - (chunk_z * RAXEL_VOXEL_CHUNK_SIZE);

    // Adjust to positive range using a positive modulo.
    local_x = ((local_x % RAXEL_VOXEL_CHUNK_SIZE) + RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
    local_y = ((local_y % RAXEL_VOXEL_CHUNK_SIZE) + RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
    local_z = ((local_z % RAXEL_VOXEL_CHUNK_SIZE) + RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;

    return local_x + local_y * RAXEL_VOXEL_CHUNK_SIZE +
           local_z * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE;
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
    raxel_coord_t chunk_x, chunk_y, chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world, x, y, z, &chunk_x, &chunk_y, &chunk_z);
    raxel_voxel_chunk_t *chunk = raxel_voxel_world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        raxel_voxel_t empty = {0};
        return empty;
    }
    raxel_size_t index = __raxel_voxel_world_from_world_to_index(world, x, y, z);
    return chunk->voxels[index];
}

// Places a voxel at the given world coordinates by updating the appropriate chunk.
// If the chunk is not loaded, nothing is done (alternatively, you could choose to load it).
void raxel_voxel_world_place_voxel(raxel_voxel_world_t *world,
                                   raxel_coord_t x,
                                   raxel_coord_t y,
                                   raxel_coord_t z,
                                   raxel_voxel_t voxel) {
    raxel_coord_t chunk_x, chunk_y, chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world, x, y, z, &chunk_x, &chunk_y, &chunk_z);
    raxel_voxel_chunk_t *chunk = raxel_voxel_world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        // we'll need to create a new chunk
        // RAXEL_CORE_LOG("Creating new chunk at (%d, %d, %d)\n", chunk_x, chunk_y, chunk_z);
        chunk = __raxel_voxel_world_create_chunk(world, chunk_x, chunk_y, chunk_z);
        if (!chunk) {
            RAXEL_CORE_FATAL_ERROR("Failed to create chunk at (%d, %d, %d)\n", chunk_x, chunk_y, chunk_z);
        }
    }
    raxel_size_t index = __raxel_voxel_world_from_world_to_index(world, x, y, z);
    // RAXEL_CORE_LOG("Placing voxel at (%d, %d, %d) in chunk (%d, %d, %d) at index %d\n", x, y, z, chunk_x, chunk_y, chunk_z, index);
    chunk->voxels[index] = voxel;
}

// -----------------------------------------------------------------------------
// Voxel World Update (Chunk Loading / Unloading)
// -----------------------------------------------------------------------------

// Updates the voxel world by unloading chunks that are too far from the camera and
// loading new chunks within the view distance. The loaded chunks (and their meta)
// remain contiguous in the arrays (i.e. indices [0, __num_loaded_chunks-1]).
void raxel_voxel_world_update(raxel_voxel_world_t *world,
                              raxel_voxel_world_update_options_t *options, raxel_compute_shader_t *compute_shader,
                              raxel_pipeline_t *pipeline) {
    // Determine the camera's chunk coordinates.
    // RAXEL_CORE_LOG("Updating voxel world\n");
    raxel_coord_t cam_chunk_x, cam_chunk_y, cam_chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world, options->camera_position[0], options->camera_position[1],
                                                   options->camera_position[2], &cam_chunk_x, &cam_chunk_y, &cam_chunk_z);

    raxel_coord_t prev_cam_chunk_x, prev_cam_chunk_y, prev_cam_chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world, world->prev_update_options.camera_position[0],
                                                   world->prev_update_options.camera_position[1],
                                                   world->prev_update_options.camera_position[2], &prev_cam_chunk_x,
                                                   &prev_cam_chunk_y, &prev_cam_chunk_z);

    world->prev_update_options = *options;

    // If the camera has moved to a new chunk, we need to update the loaded chunks.
    // Otherwise, if we are basically in the same chunk, we don't need to do anything.
    if (cam_chunk_x == prev_cam_chunk_x && cam_chunk_y == prev_cam_chunk_y && cam_chunk_z == prev_cam_chunk_z) {
        return;
    }

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

        if (num_loaded_chunks >= RAXEL_MAX_LOADED_CHUNKS) {
            break;
        }
    }
    world->__num_loaded_chunks = num_loaded_chunks;

    // // print out some information about the voxel world
    // printf("Loaded chunks: %d\n", world->__num_loaded_chunks);
    // for (raxel_size_t i = 0; i < world->__num_loaded_chunks; i++) {
    //     raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
    //     printf("Chunk %d: (%d, %d, %d)\n", i, meta->x, meta->y, meta->z);

    //     // count the number of non-air voxels in this chunk
    //     int count = 0;
    //     raxel_voxel_chunk_t *chunk = &world->chunks[i];

    //     for (raxel_size_t j = 0; j < RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE; j++) {
    //         if (chunk->voxels[j].material != 0) {
    //             count++;
    //         }
    //     }

    //     printf("  Non-air voxels: %d\n", count);
    // }

    RAXEL_CORE_LOG("Dispatching updated chunks!\n");
    raxel_voxel_world_dispatch_sb(world, compute_shader, pipeline);
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
    // TECHNICALLY, this is undefined behavior
    // but we know that sb_buffer->data is can be cast to __raxel_voxel_world_gpu_t safely
    // this is because we described the buffer as having a size of sizeof(__raxel_voxel_world_gpu_t)
    // and we know that the data pointer is pointing to a buffer of that size
    __raxel_voxel_world_gpu_t *gpu_world = compute_shader->sb_buffer->data;
    gpu_world->num_loaded_chunks = world->__num_loaded_chunks;
    for (raxel_size_t i = 0; i < world->__num_loaded_chunks; i++) {
        gpu_world->chunk_meta[i] = world->chunk_meta[i];
        memccpy(&gpu_world->chunks[i], &world->chunks[i], 1, sizeof(raxel_voxel_chunk_t));
    }

    raxel_sb_buffer_update(compute_shader->sb_buffer, pipeline);
}