// raxel_voxel.c

#include "raxel_voxel.h"
#include <raxel/core/graphics/passes/compute_pass.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>

// -----------------------------------------------------------------------------
// Voxel World Creation / Destruction
// -----------------------------------------------------------------------------

raxel_voxel_world_t *raxel_voxel_world_create(raxel_allocator_t *allocator) {
    raxel_voxel_world_t *world = raxel_malloc(allocator, sizeof(raxel_voxel_world_t));
    world->allocator = *allocator;
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
    raxel_free(&world->allocator, world);
}

// -----------------------------------------------------------------------------
// Materials
// -----------------------------------------------------------------------------

void raxel_voxel_world_add_material(raxel_voxel_world_t *world, raxel_string_t name, vec4 color) {
    raxel_voxel_material_t material;
    // Here we simply assign the name (a shallow copy). You may wish to duplicate it.
    material.name = name;
    glm_vec4_copy(color, material.color);
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

// Returns a pointer to a chunk given its chunk coordinates (relative to chunk grid).
// Only loaded chunks (the first __num_loaded_chunks in the lists) are searched.
raxel_voxel_chunk_t *raxel_voxel_world_get_chunk(raxel_voxel_world_t *world,
                                                 raxel_coord_t x,
                                                 raxel_coord_t y,
                                                 raxel_coord_t z) {
    for (raxel_size_t i = 0; i < world->__num_loaded_chunks; i++) {
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
    return chunk->voxels[local_x][local_y][local_z];
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
        // Optionally, create and load the chunk here.
        return;
    }
    raxel_coord_t local_x = x % RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t local_y = y % RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t local_z = z % RAXEL_VOXEL_CHUNK_SIZE;
    chunk->voxels[local_x][local_y][local_z] = voxel;
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
    raxel_coord_t cam_chunk_x = options->camera_position[0] / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t cam_chunk_y = options->camera_position[1] / RAXEL_VOXEL_CHUNK_SIZE;
    raxel_coord_t cam_chunk_z = options->camera_position[2] / RAXEL_VOXEL_CHUNK_SIZE;

    // Unload chunks that are farther than the view distance.
    for (raxel_size_t i = 0; i < world->__num_loaded_chunks;) {
        raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
        int dx = meta->x - cam_chunk_x;
        int dy = meta->y - cam_chunk_y;
        int dz = meta->z - cam_chunk_z;
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (distance > options->view_distance) {
            // Remove this chunk by swapping with the last loaded chunk.
            world->chunk_meta[i] = world->chunk_meta[world->__num_loaded_chunks - 1];
            world->chunks[i] = world->chunks[world->__num_loaded_chunks - 1];
            world->__num_loaded_chunks--;
        } else {
            i++;
        }
    }

    // Load new chunks within the view distance.
    int range = (int)ceil(options->view_distance);
    for (int x = cam_chunk_x - range; x <= cam_chunk_x + range; x++) {
        for (int y = cam_chunk_y - range; y <= cam_chunk_y + range; y++) {
            for (int z = cam_chunk_z - range; z <= cam_chunk_z + range; z++) {
                int dx = x - cam_chunk_x;
                int dy = y - cam_chunk_y;
                int dz = z - cam_chunk_z;
                float distance = sqrtf(dx * dx + dy * dy + dz * dz);
                if (distance <= options->view_distance) {
                    // Check if the chunk is already loaded.
                    int found = 0;
                    for (raxel_size_t i = 0; i < world->__num_loaded_chunks; i++) {
                        raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
                        if (meta->x == x && meta->y == y && meta->z == z) {
                            found = 1;
                            break;
                        }
                    }
                    if (!found && world->__num_loaded_chunks < RAXEL_MAX_LOADED_CHUNKS) {
                        // Create a new chunk.
                        raxel_voxel_chunk_t new_chunk;
                        memset(&new_chunk, 0, sizeof(new_chunk));
                        raxel_list_push_back(world->chunks, new_chunk);

                        // Create corresponding chunk meta.
                        raxel_voxel_chunk_meta_t new_meta;
                        new_meta.x = x;
                        new_meta.y = y;
                        new_meta.z = z;
                        new_meta.state = RAXEL_VOXEL_CHUNK_STATE_COUNT;
                        raxel_list_push_back(world->chunk_meta, new_meta);
                        world->__num_loaded_chunks++;
                    }
                }
            }
        }
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

    // Update the storage buffer with the new voxel world data.
    memcpy(compute_shader->sb_buffer->data, &gpu_world, sizeof(__raxel_voxel_world_gpu_t));
    raxel_sb_buffer_update(compute_shader->sb_buffer, pipeline);
}