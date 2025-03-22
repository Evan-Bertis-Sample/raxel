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


static inline raxel_bvh_bounds_t __raxel_bounds3f_empty(void) {
    raxel_bvh_bounds_t b;
    b.min[0] = b.min[1] = b.min[2] = 1e30f;
    b.max[0] = b.max[1] = b.max[2] = -1e30f;
    return b;
}

static inline raxel_bvh_bounds_t __raxel_bounds3f_union(const raxel_bvh_bounds_t *b, const raxel_bvh_bounds_t *c) {
    raxel_bvh_bounds_t ret;
    ret.min[0] = (b->min[0] < c->min[0]) ? b->min[0] : c->min[0];
    ret.min[1] = (b->min[1] < c->min[1]) ? b->min[1] : c->min[1];
    ret.min[2] = (b->min[2] < c->min[2]) ? b->min[2] : c->min[2];
    ret.max[0] = (b->max[0] > c->max[0]) ? b->max[0] : c->max[0];
    ret.max[1] = (b->max[1] > c->max[1]) ? b->max[1] : c->max[1];
    ret.max[2] = (b->max[2] > c->max[2]) ? b->max[2] : c->max[2];
    return ret;
}

// Instead of returning a vec3, write the centroid into out_centroid.
static inline void __raxel_bounds3f_centroid(const raxel_bvh_bounds_t *b, vec3 out_centroid) {
    out_centroid[0] = 0.5f * (b->min[0] + b->max[0]);
    out_centroid[1] = 0.5f * (b->min[1] + b->max[1]);
    out_centroid[2] = 0.5f * (b->min[2] + b->max[2]);
}

// ----------------------------------------------------------------------
// BVH Build (temporary tree)
// ----------------------------------------------------------------------
static raxel_bvh_build_node_t *__raxel_bvh_build_node_create(raxel_allocator_t *allocator) {
    raxel_bvh_build_node_t *node = (raxel_bvh_build_node_t *)raxel_malloc(allocator, sizeof(raxel_bvh_build_node_t));
    node->children[0] = node->children[1] = NULL;
    node->n_primitives = 0;
    node->first_prim_offset = -1;
    node->split_axis = 0;
    node->bounds = __raxel_bounds3f_empty();
    return node;
}

static void __raxel_bvh_build_tree_free(raxel_bvh_build_node_t *node, raxel_allocator_t *allocator) {
    if (!node) return;
    __raxel_bvh_build_tree_free(node->children[0], allocator);
    __raxel_bvh_build_tree_free(node->children[1], allocator);
    raxel_free(allocator, node);
}

static raxel_bvh_build_node_t *build_raxel_bvh(raxel_bvh_bounds_t *primitive_bounds, int *primitive_indices,
                                               int start, int end, int max_leaf_size,
                                               raxel_allocator_t *allocator) {
    raxel_bvh_build_node_t *node = __raxel_bvh_build_node_create(allocator);
    raxel_bvh_bounds_t bounds = __raxel_bounds3f_empty();
    for (int i = start; i < end; i++) {
        bounds = __raxel_bounds3f_union(&bounds, &primitive_bounds[primitive_indices[i]]);
    }
    node->bounds = bounds;
    int n_primitives = end - start;
    if (n_primitives <= max_leaf_size) {
        // Leaf node.
        node->n_primitives = n_primitives;
        node->first_prim_offset = start;
        return node;
    } else {
        // Compute centroid bounds.
        raxel_bvh_bounds_t centroid_bounds = __raxel_bounds3f_empty();
        for (int i = start; i < end; i++) {
            vec3 centroid;
            __raxel_bounds3f_centroid(&primitive_bounds[primitive_indices[i]], centroid);
            raxel_bvh_bounds_t tmp;
            glm_vec3_copy(centroid, tmp.min);
            glm_vec3_copy(centroid, tmp.max);
            centroid_bounds = __raxel_bounds3f_union(&centroid_bounds, &tmp);
        }
        // Choose split axis as the largest extent.
        vec3 extent;
        extent[0] = centroid_bounds.max[0] - centroid_bounds.min[0];
        extent[1] = centroid_bounds.max[1] - centroid_bounds.min[1];
        extent[2] = centroid_bounds.max[2] - centroid_bounds.min[2];
        int axis = 0;
        if (extent[1] > extent[0]) axis = 1;
        if (extent[2] > ((axis == 0) ? extent[0] : extent[1])) axis = 2;
        node->split_axis = axis;
        int mid = (start + end) / 2;
        // Partition primitives by median.
        int cmp(const void *a, const void *b) {
            int ia = *(const int *)a;
            int ib = *(const int *)b;
            vec3 ca, cb;
            raxel_bounds3f_centroid(&primitive_bounds[ia], ca);
            raxel_bounds3f_centroid(&primitive_bounds[ib], cb);
            float diff = (axis == 0 ? ca[0] - cb[0] : axis == 1 ? ca[1] - cb[1]
                                                                : ca[2] - cb[2]);
            return (diff < 0) ? -1 : (diff > 0) ? 1
                                                : 0;
        }
        qsort(primitive_indices + start, n_primitives, sizeof(int), cmp);
        node->children[0] = build_raxel_bvh(primitive_bounds, primitive_indices, start, mid, max_leaf_size, allocator);
        node->children[1] = build_raxel_bvh(primitive_bounds, primitive_indices, mid, end, max_leaf_size, allocator);
        return node;
    }
}

static int __count_raxel_bvh_nodes(raxel_bvh_build_node_t *node) {
    if (!node) return 0;
    if (node->n_primitives > 0)
        return 1;
    return 1 + __count_raxel_bvh_nodes(node->children[0]) + __count_raxel_bvh_nodes(node->children[1]);
}

static int __flatten_bvh_tree(raxel_bvh_build_node_t *node, int *offset, raxel_linear_bvh_node_t *nodes) {
    int my_offset = (*offset);
    raxel_linear_bvh_node_t *linear_node = &nodes[my_offset];
    (*offset)++;
    linear_node->bounds = node->bounds;
    if (node->n_primitives > 0) {
        linear_node->primitives_offset = node->first_prim_offset;
        linear_node->n_primitives = (uint16_t)node->n_primitives;
    } else {
        linear_node->axis = (uint8_t)node->split_axis;
        linear_node->n_primitives = 0;
        __flatten_bvh_tree(node->children[0], offset, nodes);
        linear_node->second_child_offset = __flatten_bvh_tree(node->children[1], offset, nodes);
    }
    return my_offset;
}

// ----------------------------------------------------------------------
// Public BVH Accelerator API
// ----------------------------------------------------------------------

// Build the BVH from an array of primitive bounds and indices.
raxel_bvh_accel_t *raxel_bvh_accel_build(raxel_bvh_bounds_t *primitive_bounds, int *primitive_indices, int n, int max_leaf_size, raxel_allocator_t *allocator) {
    raxel_bvh_accel_t *bvh = (raxel_bvh_accel_t *)raxel_malloc(allocator, sizeof(raxel_bvh_accel_t));
    bvh->max_leaf_size = max_leaf_size;
    raxel_bvh_build_node_t *root = build_raxel_bvh(primitive_bounds, primitive_indices, 0, n, max_leaf_size, allocator);
    bvh->n_nodes = __count_raxel_bvh_nodes(root);
    bvh->nodes = (raxel_linear_bvh_node_t *)raxel_malloc(allocator, bvh->n_nodes * sizeof(raxel_linear_bvh_node_t));
    int offset = 0;
    __flatten_bvh_tree(root, &offset, bvh->nodes);
    __raxel_bvh_build_tree_free(root, allocator);
    return bvh;
}

void raxel_bvh_accel_destroy(raxel_bvh_accel_t *bvh, raxel_allocator_t *allocator) {
    if (!bvh) return;
    if (bvh->nodes)
        raxel_free(allocator, bvh->nodes);
    raxel_free(allocator, bvh);
}

raxel_bvh_accel_t *raxel_bvh_accel_build_from_voxel_world(raxel_voxel_world *world, int max_leaf_size, raxel_allocator_t *allocator) {
    // First, count the total number of non-empty voxels.
    int total_prims = 0;
    for (size_t i = 0; i < world->__num_loaded_chunks; i++) {
        raxel_voxel_chunk_t *chunk = &world->chunks[i];
        // Compute the chunk origin using transformation utilities.
        raxel_voxel_chunk_meta_t meta = world->chunk_meta[i];
        vec3 chunk_origin = {(float)meta.x, (float)meta.y, (float)meta.z};
        glm_vec3_scale(chunk_origin, (float)RAXEL_VOXEL_CHUNK_SIZE, chunk_origin);
        for (int j = 0; j < RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE; j++) {
            if (chunk->voxels[j].material != 0) {
                total_prims++;
            }
        }
    }
    if (total_prims == 0)
        return NULL;

    // Allocate arrays for primitive bounds and indices.
    raxel_bvh_bounds_t *prim_bounds = (raxel_bvh_bounds_t *)raxel_malloc(allocator, total_prims * sizeof(raxel_bvh_bounds_t));
    int *prim_indices = (int *)raxel_malloc(allocator, total_prims * sizeof(int));

    int index = 0;
    // For each non-empty voxel, compute its bounding box.
    for (size_t i = 0; i < world->__num_loaded_chunks; i++) {
        raxel_voxel_chunk_meta_t meta = world->chunk_meta[i];
        raxel_voxel_chunk_t *chunk = &world->chunks[i];
        // Compute the chunk origin using cglm utilities.
        vec3 chunk_origin = {(float)meta.x, (float)meta.y, (float)meta.z};
        glm_vec3_scale(chunk_origin, (float)RAXEL_VOXEL_CHUNK_SIZE, chunk_origin);

        for (int j = 0; j < RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE; j++) {
            if (chunk->voxels[j].material == 0)
                continue;
            // Compute local voxel coordinates.
            int lx = j % RAXEL_VOXEL_CHUNK_SIZE;
            int ly = (j / RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
            int lz = j / (RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE);
            // Compute world coordinates for this voxel.
            float min_pt[3];
            min_pt[0] = chunk_origin[0] + (float)lx;
            min_pt[1] = chunk_origin[1] + (float)ly;
            min_pt[2] = chunk_origin[2] + (float)lz;
            float max_pt[3];
            max_pt[0] = min_pt[0] + 1.0f;
            max_pt[1] = min_pt[1] + 1.0f;
            max_pt[2] = min_pt[2] + 1.0f;
            // Set bounding box.
            prim_bounds[index].min[0] = min_pt[0];
            prim_bounds[index].min[1] = min_pt[1];
            prim_bounds[index].min[2] = min_pt[2];
            prim_bounds[index].max[0] = max_pt[0];
            prim_bounds[index].max[1] = max_pt[1];
            prim_bounds[index].max[2] = max_pt[2];
            // Store the primitive index (here we simply use a running index)
            prim_indices[index] = index;
            index++;
        }
    }
    // Build the BVH accelerator.
    raxel_bvh_accel_t *bvh = raxel_bvh_accel_build(prim_bounds, prim_indices, total_prims, max_leaf_size, allocator);
    // Free temporary arrays.
    raxel_free(allocator, prim_bounds);
    raxel_free(allocator, prim_indices);
    return bvh;
}
