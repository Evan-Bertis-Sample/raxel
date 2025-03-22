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
    raxel_list_push_back(world->chunk_meta, meta);
    raxel_voxel_chunk_t chunk;
    raxel_list_push_back(world->chunks, chunk);
    raxel_voxel_chunk_t *new_chunk = &world->chunks[raxel_list_size(world->chunks) - 1];
    memset(new_chunk, 0, sizeof(raxel_voxel_chunk_t));
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
    raxel_coord_t local_x = x - (chunk_x * RAXEL_VOXEL_CHUNK_SIZE);
    raxel_coord_t local_y = y - (chunk_y * RAXEL_VOXEL_CHUNK_SIZE);
    raxel_coord_t local_z = z - (chunk_z * RAXEL_VOXEL_CHUNK_SIZE);
    local_x = ((local_x % RAXEL_VOXEL_CHUNK_SIZE) + RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
    local_y = ((local_y % RAXEL_VOXEL_CHUNK_SIZE) + RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
    local_z = ((local_z % RAXEL_VOXEL_CHUNK_SIZE) + RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
    return local_x + local_y * RAXEL_VOXEL_CHUNK_SIZE +
           local_z * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE;
}

raxel_voxel_chunk_t *raxel_voxel_world_get_chunk(raxel_voxel_world_t *world,
                                                 raxel_coord_t x,
                                                 raxel_coord_t y,
                                                 raxel_coord_t z) {
    for (raxel_size_t i = 0; i < raxel_list_size(world->chunk_meta); i++) {
        raxel_voxel_chunk_meta_t *meta = &world->chunk_meta[i];
        if (meta->x == x && meta->y == y && meta->z == z) {
            return &world->chunks[i];
        }
    }
    return NULL;
}

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

void raxel_voxel_world_place_voxel(raxel_voxel_world_t *world,
                                   raxel_coord_t x,
                                   raxel_coord_t y,
                                   raxel_coord_t z,
                                   raxel_voxel_t voxel) {
    raxel_coord_t chunk_x, chunk_y, chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world, x, y, z, &chunk_x, &chunk_y, &chunk_z);
    raxel_voxel_chunk_t *chunk = raxel_voxel_world_get_chunk(world, chunk_x, chunk_y, chunk_z);
    if (!chunk) {
        chunk = __raxel_voxel_world_create_chunk(world, chunk_x, chunk_y, chunk_z);
        if (!chunk) {
            RAXEL_CORE_FATAL_ERROR("Failed to create chunk at (%d, %d, %d)\n", chunk_x, chunk_y, chunk_z);
        }
    }
    raxel_size_t index = __raxel_voxel_world_from_world_to_index(world, x, y, z);
    chunk->voxels[index] = voxel;
}

void raxel_voxel_world_set_sb(raxel_voxel_world_t *world,
                              raxel_compute_shader_t *compute_shader,
                              raxel_pipeline_t *pipeline) {
    raxel_sb_buffer_desc_t sb_desc = RAXEL_SB_DESC(
        (raxel_sb_entry_t){.name = "voxel_world", .offset = 0, .size = sizeof(__raxel_voxel_world_gpu_t)});
    raxel_compute_shader_set_sb(compute_shader, pipeline, &sb_desc);
}

// -----------------------------------------------------------------------------
// BVH Utility Functions
// -----------------------------------------------------------------------------
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

static inline void __raxel_bounds3f_centroid(const raxel_bvh_bounds_t *b, vec3 out_centroid) {
    out_centroid[0] = 0.5f * (b->min[0] + b->max[0]);
    out_centroid[1] = 0.5f * (b->min[1] + b->max[1]);
    out_centroid[2] = 0.5f * (b->min[2] + b->max[2]);
}

// -----------------------------------------------------------------------------
// BVH Build (Temporary Tree) with Node Limit
// -----------------------------------------------------------------------------
static __raxel_bvh_build_node_t *__raxel_bvh_build_node_create(raxel_allocator_t *allocator) {
    __raxel_bvh_build_node_t *node = (__raxel_bvh_build_node_t *)raxel_malloc(allocator, sizeof(__raxel_bvh_build_node_t));
    node->children[0] = node->children[1] = NULL;
    node->n_primitives = 0;
    node->first_prim_offset = -1;
    node->split_axis = 0;
    node->bounds = __raxel_bounds3f_empty();
    return node;
}

static void __raxel_bvh_build_tree_free(__raxel_bvh_build_node_t *node, raxel_allocator_t *allocator) {
    if (!node) return;
    __raxel_bvh_build_tree_free(node->children[0], allocator);
    __raxel_bvh_build_tree_free(node->children[1], allocator);
    raxel_free(allocator, node);
}

static __raxel_bvh_build_node_t *__build_raxel_bvh_limited(raxel_bvh_bounds_t *primitive_bounds,
                                                           int *primitive_indices,
                                                           int start,
                                                           int end,
                                                           int max_leaf_size,
                                                           int *node_counter,
                                                           raxel_allocator_t *allocator) {
    __raxel_bvh_build_node_t *node = __raxel_bvh_build_node_create(allocator);
    raxel_bvh_bounds_t bounds = __raxel_bounds3f_empty();
    for (int i = start; i < end; i++) {
        bounds = __raxel_bounds3f_union(&bounds, &primitive_bounds[primitive_indices[i]]);
    }
    node->bounds = bounds;
    int n_primitives = end - start;
    // Force leaf if too few primitives or adding children would exceed max nodes.
    if (n_primitives <= max_leaf_size || (*node_counter + 2 > RAXEL_BVH_MAX_NODES)) {
        node->n_primitives = n_primitives;
        node->first_prim_offset = start;
        return node;
    } else {
        raxel_bvh_bounds_t centroid_bounds = __raxel_bounds3f_empty();
        for (int i = start; i < end; i++) {
            vec3 centroid;
            __raxel_bounds3f_centroid(&primitive_bounds[primitive_indices[i]], centroid);
            raxel_bvh_bounds_t tmp;
            glm_vec3_copy(centroid, tmp.min);
            glm_vec3_copy(centroid, tmp.max);
            centroid_bounds = __raxel_bounds3f_union(&centroid_bounds, &tmp);
        }
        vec3 extent;
        extent[0] = centroid_bounds.max[0] - centroid_bounds.min[0];
        extent[1] = centroid_bounds.max[1] - centroid_bounds.min[1];
        extent[2] = centroid_bounds.max[2] - centroid_bounds.min[2];
        int axis = 0;
        if (extent[1] > extent[0]) axis = 1;
        if (extent[2] > ((axis == 0) ? extent[0] : extent[1])) axis = 2;
        node->split_axis = axis;
        int mid = (start + end) / 2;
        int cmp(const void *a, const void *b) {
            int ia = *(const int *)a;
            int ib = *(const int *)b;
            vec3 ca, cb;
            __raxel_bounds3f_centroid(&primitive_bounds[ia], ca);
            __raxel_bounds3f_centroid(&primitive_bounds[ib], cb);
            float diff = (axis == 0 ? ca[0] - cb[0] : axis == 1 ? ca[1] - cb[1]
                                                                : ca[2] - cb[2]);
            return (diff < 0) ? -1 : (diff > 0) ? 1
                                                : 0;
        }
        qsort(primitive_indices + start, n_primitives, sizeof(int), cmp);
        *node_counter += 2;
        node->children[0] = __build_raxel_bvh_limited(primitive_bounds, primitive_indices,
                                                      start, mid, max_leaf_size, node_counter, allocator);
        node->children[1] = __build_raxel_bvh_limited(primitive_bounds, primitive_indices,
                                                      mid, end, max_leaf_size, node_counter, allocator);
        return node;
    }
}

static int __count_raxel_bvh_nodes(__raxel_bvh_build_node_t *node) {
    if (!node) return 0;
    if (node->n_primitives > 0)
        return 1;
    return 1 + __count_raxel_bvh_nodes(node->children[0]) + __count_raxel_bvh_nodes(node->children[1]);
}

static int __flatten_bvh_tree(__raxel_bvh_build_node_t *node, int *offset, raxel_linear_bvh_node_t *nodes) {
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

static void __print_bvh_build_structure(__raxel_bvh_build_node_t *node, int depth) {
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }
    if (node->n_primitives > 0) {
        printf("Leaf: %d primitives\n", node->n_primitives);
    } else {
        printf("Interior: Axis %d\n", node->split_axis);
        __print_bvh_build_structure(node->children[0], depth + 1);
        __print_bvh_build_structure(node->children[1], depth + 1);
    }
}

// -----------------------------------------------------------------------------
// Public BVH Accelerator API
// ----------------------------------------------------------------------------

raxel_bvh_accel_t *raxel_bvh_accel_build(raxel_bvh_bounds_t *primitive_bounds,
                                         int *primitive_indices,
                                         int n,
                                         int max_leaf_size,
                                         raxel_allocator_t *allocator) {
    raxel_bvh_accel_t *bvh = (raxel_bvh_accel_t *)raxel_malloc(allocator, sizeof(raxel_bvh_accel_t));
    bvh->max_leaf_size = max_leaf_size;
    int node_counter = 1;
    __raxel_bvh_build_node_t *root = __build_raxel_bvh_limited(primitive_bounds, primitive_indices, 0, n, max_leaf_size, &node_counter, allocator);
    __print_bvh_build_structure(root, 0);
    bvh->n_nodes = __count_raxel_bvh_nodes(root);
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

raxel_bvh_accel_t *raxel_bvh_accel_build_from_voxel_world(raxel_voxel_world_t *world, int max_leaf_size, raxel_allocator_t *allocator) {
    int total_prims = 0;
    for (size_t i = 0; i < world->__num_loaded_chunks; i++) {
        raxel_voxel_chunk_t *chunk = &world->chunks[i];
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

    raxel_bvh_bounds_t *prim_bounds = (raxel_bvh_bounds_t *)raxel_malloc(allocator, total_prims * sizeof(raxel_bvh_bounds_t));
    int *prim_indices = (int *)raxel_malloc(allocator, total_prims * sizeof(int));

    int index = 0;
    for (size_t i = 0; i < world->__num_loaded_chunks; i++) {
        raxel_voxel_chunk_meta_t meta = world->chunk_meta[i];
        raxel_voxel_chunk_t *chunk = &world->chunks[i];
        vec3 chunk_origin = {(float)meta.x, (float)meta.y, (float)meta.z};
        glm_vec3_scale(chunk_origin, (float)RAXEL_VOXEL_CHUNK_SIZE, chunk_origin);
        for (int j = 0; j < RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE; j++) {
            if (chunk->voxels[j].material == 0)
                continue;
            int lx = j % RAXEL_VOXEL_CHUNK_SIZE;
            int ly = (j / RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
            int lz = j / (RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE);
            float min_pt[3];
            min_pt[0] = chunk_origin[0] + (float)lx;
            min_pt[1] = chunk_origin[1] + (float)ly;
            min_pt[2] = chunk_origin[2] + (float)lz;
            float max_pt[3];
            max_pt[0] = min_pt[0] + 1.0f;
            max_pt[1] = min_pt[1] + 1.0f;
            max_pt[2] = min_pt[2] + 1.0f;
            prim_bounds[index].min[0] = min_pt[0];
            prim_bounds[index].min[1] = min_pt[1];
            prim_bounds[index].min[2] = min_pt[2];
            prim_bounds[index].max[0] = max_pt[0];
            prim_bounds[index].max[1] = max_pt[1];
            prim_bounds[index].max[2] = max_pt[2];
            prim_indices[index] = index;
            index++;
        }
    }

    raxel_bvh_accel_t *bvh = raxel_bvh_accel_build(prim_bounds, prim_indices, total_prims, max_leaf_size, allocator);
    raxel_free(allocator, prim_bounds);
    raxel_free(allocator, prim_indices);
    return bvh;
}

void raxel_bvh_accel_print(raxel_bvh_accel_t *bvh) {
    printf("BVH Accelerator:\n");
    printf("  Max Leaf Size: %d\n", bvh->max_leaf_size);
    printf("  Num Nodes: %d\n", bvh->n_nodes);
    for (int i = 0; i < bvh->n_nodes; i++) {
        raxel_linear_bvh_node_t *node = &bvh->nodes[i];
        printf("  Node %d:\n", i);
        printf("    Bounds: (%f, %f, %f) - (%f, %f, %f)\n", node->bounds.min[0], node->bounds.min[1], node->bounds.min[2],
               node->bounds.max[0], node->bounds.max[1], node->bounds.max[2]);
        if (node->n_primitives > 0) {
            printf("    Leaf Node: %d primitives starting at %d\n", node->n_primitives, node->primitives_offset);
        } else {
            printf("    Interior Node: Split Axis %d, Second Child Offset %d\n", node->axis, node->second_child_offset);
        }
    }
}

// -----------------------------------------------------------------------------
// Voxel World Update (Chunk Loading / Unloading)
// -----------------------------------------------------------------------------
void raxel_voxel_world_update(raxel_voxel_world_t *world,
                              raxel_voxel_world_update_options_t *options,
                              raxel_compute_shader_t *compute_shader,
                              raxel_pipeline_t *pipeline) {
    raxel_coord_t cam_chunk_x, cam_chunk_y, cam_chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world,
                                                   options->camera_position[0],
                                                   options->camera_position[1],
                                                   options->camera_position[2],
                                                   &cam_chunk_x, &cam_chunk_y, &cam_chunk_z);

    raxel_coord_t prev_cam_chunk_x, prev_cam_chunk_y, prev_cam_chunk_z;
    __raxel_voxel_world_from_world_to_chunk_coords(world,
                                                   world->prev_update_options.camera_position[0],
                                                   world->prev_update_options.camera_position[1],
                                                   world->prev_update_options.camera_position[2],
                                                   &prev_cam_chunk_x, &prev_cam_chunk_y, &prev_cam_chunk_z);

    world->prev_update_options = *options;

    if (cam_chunk_x == prev_cam_chunk_x &&
        cam_chunk_y == prev_cam_chunk_y &&
        cam_chunk_z == prev_cam_chunk_z) {
        return;
    }

    raxel_size_t num_chunks = raxel_list_size(world->chunk_meta);
    raxel_size_t num_loaded_chunks = 0;
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

    // --- Build the BVH from the currently loaded chunks ---
    int max_leaf_size = 4;
    raxel_bvh_accel_t *bvh = raxel_bvh_accel_build_from_voxel_world(world, max_leaf_size, world->allocator);

    // Update GPU world buffer with voxel data and BVH.
    __raxel_voxel_world_gpu_t *gpu_world = compute_shader->sb_buffer->data;
    gpu_world->num_loaded_chunks = world->__num_loaded_chunks;
    for (raxel_size_t i = 0; i < world->__num_loaded_chunks; i++) {
        gpu_world->chunk_meta[i] = world->chunk_meta[i];
        memccpy(&gpu_world->chunks[i], &world->chunks[i], 1, sizeof(raxel_voxel_chunk_t));
    }
    memcpy(&gpu_world->bvh, bvh, sizeof(raxel_bvh_accel_t));

    // raxel_bvh_accel_print(bvh);

    raxel_bvh_accel_destroy(bvh, world->allocator);
    raxel_sb_buffer_update(compute_shader->sb_buffer, pipeline);
}
