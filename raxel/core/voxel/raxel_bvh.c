#include "raxel_bvh.h"
#include <cglm/cglm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raxel_voxel.h"  // Assumes your voxel world definitions are here

// ----------------------------------------------------------------------
// Utility functions for raxel_bvh_bounds
// ----------------------------------------------------------------------
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
            return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
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

// ----------------------------------------------------------------------
// BVH Iterator Implementation
// ----------------------------------------------------------------------
static void *bvh_it_current(raxel_iterator_t *it) {
    raxel_bvh_accel_t *bvh = (raxel_bvh_accel_t *)it->__ctx;
    uintptr_t index = (uintptr_t)it->__data;
    if (index >= (uintptr_t)bvh->n_nodes)
        return NULL;
    return &bvh->nodes[index];
}

static void *bvh_it_next(raxel_iterator_t *it) {
    raxel_bvh_accel_t *bvh = (raxel_bvh_accel_t *)it->__ctx;
    uintptr_t index = (uintptr_t)it->__data;
    index++;
    it->__data = (void *)index;
    if (index >= (uintptr_t)bvh->n_nodes)
        return NULL;
    return &bvh->nodes[index];
}

raxel_iterator_t raxel_bvh_accel_iterator(raxel_bvh_accel_t *bvh) {
    raxel_iterator_t it;
    it.__ctx = bvh;
    it.__data = (void *)(uintptr_t)0;
    it.current = bvh_it_current;
    it.next = bvh_it_next;
    return it;
}

// ----------------------------------------------------------------------
// Build BVH Accelerator from a Voxel World
// ----------------------------------------------------------------------
/*
  This function builds a BVH accelerator from a raxel_voxel_world.
  It iterates over all loaded chunks and all voxels within each chunk.
  For each non-empty voxel (material != 0), it computes a bounding box
  in world space: [x, x+1], [y, y+1], [z, z+1].
  It then gathers these into an array of primitive bounds and corresponding
  indices, and calls raxel_bvh_accel_build to build the BVH.
*/
raxel_bvh_accel_t *raxel_bvh_accel_build_from_voxel_world(raxel_voxel_world *world, int max_leaf_size, raxel_allocator_t *allocator) {
    // First, count the total number of non-empty voxels.
    int total_prims = 0;
    for (size_t i = 0; i < world->__num_loaded_chunks; i++) {
        raxel_voxel_chunk_t *chunk = &world->chunks[i];
        int chunk_origin_x = world->chunk_meta[i].x * RAXEL_VOXEL_CHUNK_SIZE;
        int chunk_origin_y = world->chunk_meta[i].y * RAXEL_VOXEL_CHUNK_SIZE;
        int chunk_origin_z = world->chunk_meta[i].z * RAXEL_VOXEL_CHUNK_SIZE;
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
        int chunk_origin_x = meta.x * RAXEL_VOXEL_CHUNK_SIZE;
        int chunk_origin_y = meta.y * RAXEL_VOXEL_CHUNK_SIZE;
        int chunk_origin_z = meta.z * RAXEL_VOXEL_CHUNK_SIZE;
        for (int j = 0; j < RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE; j++) {
            if (chunk->voxels[j].material == 0)
                continue;
            // Compute local voxel coordinates.
            int lx = j % RAXEL_VOXEL_CHUNK_SIZE;
            int ly = (j / RAXEL_VOXEL_CHUNK_SIZE) % RAXEL_VOXEL_CHUNK_SIZE;
            int lz = j / (RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE);
            // Compute world coordinates for this voxel.
            float min_x = (float)(chunk_origin_x + lx);
            float min_y = (float)(chunk_origin_y + ly);
            float min_z = (float)(chunk_origin_z + lz);
            float max_x = min_x + 1.0f;
            float max_y = min_y + 1.0f;
            float max_z = min_z + 1.0f;
            prim_bounds[index].min[0] = min_x;
            prim_bounds[index].min[1] = min_y;
            prim_bounds[index].min[2] = min_z;
            prim_bounds[index].max[0] = max_x;
            prim_bounds[index].max[1] = max_y;
            prim_bounds[index].max[2] = max_z;
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
