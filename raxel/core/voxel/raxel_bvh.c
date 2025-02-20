#include "raxel_bvh.h"

#include <cglm/cglm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ----------------------------------------------------------------------
// Utility functions for raxel_bounds3f
// ----------------------------------------------------------------------
static inline raxel_bounds3f raxel_bounds3f_empty(void) {
    raxel_bounds3f b;
    b.min[0] = b.min[1] = b.min[2] = 1e30f;
    b.max[0] = b.max[1] = b.max[2] = -1e30f;
    return b;
}

static inline raxel_bounds3f raxel_bounds3f_union(const raxel_bounds3f *b, const raxel_bounds3f *c) {
    raxel_bounds3f ret;
    ret.min[0] = (b->min[0] < c->min[0]) ? b->min[0] : c->min[0];
    ret.min[1] = (b->min[1] < c->min[1]) ? b->min[1] : c->min[1];
    ret.min[2] = (b->min[2] < c->min[2]) ? b->min[2] : c->min[2];
    ret.max[0] = (b->max[0] > c->max[0]) ? b->max[0] : c->max[0];
    ret.max[1] = (b->max[1] > c->max[1]) ? b->max[1] : c->max[1];
    ret.max[2] = (b->max[2] > c->max[2]) ? b->max[2] : c->max[2];
    return ret;
}

// Fix: Instead of returning a vec3, we write the centroid into out_centroid.
static inline void raxel_bounds3f_centroid(const raxel_bounds3f *b, vec3 out_centroid) {
    out_centroid[0] = 0.5f * (b->min[0] + b->max[0]);
    out_centroid[1] = 0.5f * (b->min[1] + b->max[1]);
    out_centroid[2] = 0.5f * (b->min[2] + b->max[2]);
}

// ----------------------------------------------------------------------
// BVH Build (temporary tree)
// ----------------------------------------------------------------------
static raxel_bvh_build_node *new_raxel_bvh_build_node(raxel_allocator_t *allocator) {
    raxel_bvh_build_node *node = (raxel_bvh_build_node *)raxel_malloc(allocator, sizeof(raxel_bvh_build_node));
    node->children[0] = node->children[1] = NULL;
    node->n_primitives = 0;
    node->first_prim_offset = -1;
    node->split_axis = 0;
    node->bounds = raxel_bounds3f_empty();
    return node;
}

static void free_raxel_bvh_build_tree(raxel_bvh_build_node *node, raxel_allocator_t *allocator) {
    if (!node) return;
    free_raxel_bvh_build_tree(node->children[0], allocator);
    free_raxel_bvh_build_tree(node->children[1], allocator);
    raxel_free(allocator, node);
}

static raxel_bvh_build_node *build_raxel_bvh(raxel_bounds3f *primitive_bounds, int *primitive_indices,
                                             int start, int end, int max_leaf_size,
                                             raxel_allocator_t *allocator) {
    raxel_bvh_build_node *node = new_raxel_bvh_build_node(allocator);
    raxel_bounds3f bounds = raxel_bounds3f_empty();
    for (int i = start; i < end; i++) {
        bounds = raxel_bounds3f_union(&bounds, &primitive_bounds[primitive_indices[i]]);
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
        raxel_bounds3f centroid_bounds = raxel_bounds3f_empty();
        for (int i = start; i < end; i++) {
            vec3 centroid;
            raxel_bounds3f_centroid(&primitive_bounds[primitive_indices[i]], centroid);
            raxel_bounds3f tmp;
            glm_vec3_copy(centroid, tmp.min);
            glm_vec3_copy(centroid, tmp.max);
            centroid_bounds = raxel_bounds3f_union(&centroid_bounds, &tmp);
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

static int count_raxel_bvh_nodes(raxel_bvh_build_node *node) {
    if (!node) return 0;
    if (node->n_primitives > 0)
        return 1;
    return 1 + count_raxel_bvh_nodes(node->children[0]) + count_raxel_bvh_nodes(node->children[1]);
}

static int flatten_bvh_tree(raxel_bvh_build_node *node, int *offset, raxel_linear_bvh_node *nodes) {
    int my_offset = (*offset);
    raxel_linear_bvh_node *linear_node = &nodes[my_offset];
    (*offset)++;
    linear_node->bounds = node->bounds;
    if (node->n_primitives > 0) {
        linear_node->primitives_offset = node->first_prim_offset;
        linear_node->n_primitives = node->n_primitives;
    } else {
        linear_node->axis = (uint8_t)node->split_axis;
        linear_node->n_primitives = 0;
        flatten_bvh_tree(node->children[0], offset, nodes);
        linear_node->second_child_offset = flatten_bvh_tree(node->children[1], offset, nodes);
    }
    return my_offset;
}

// ----------------------------------------------------------------------
// Public BVH Accelerator API
// ----------------------------------------------------------------------
raxel_bvh_accel *raxel_bvh_accel_build(raxel_bounds3f *primitive_bounds, int *primitive_indices, int n, int max_leaf_size, raxel_allocator_t *allocator) {
    raxel_bvh_accel *bvh = (raxel_bvh_accel *)raxel_malloc(allocator, sizeof(raxel_bvh_accel));
    bvh->max_leaf_size = max_leaf_size;
    raxel_bvh_build_node *root = build_raxel_bvh(primitive_bounds, primitive_indices, 0, n, max_leaf_size, allocator);
    bvh->n_nodes = count_raxel_bvh_nodes(root);
    bvh->nodes = (raxel_linear_bvh_node *)raxel_malloc(allocator, bvh->n_nodes * sizeof(raxel_linear_bvh_node));
    int offset = 0;
    flatten_bvh_tree(root, &offset, bvh->nodes);
    free_raxel_bvh_build_tree(root, allocator);
    return bvh;
}

static int raxel_bounds3f_intersect_p(const raxel_bounds3f *b, const raxel_ray *ray, const vec3 inv_dir, const int dir_is_neg[3]) {
    float tmin = ((dir_is_neg[0] ? b->max[0] : b->min[0]) - ray->o[0]) * inv_dir[0];
    float tmax = ((dir_is_neg[0] ? b->min[0] : b->max[0]) - ray->o[0]) * inv_dir[0];
    float tymin = ((dir_is_neg[1] ? b->max[1] : b->min[1]) - ray->o[1]) * inv_dir[1];
    float tymax = ((dir_is_neg[1] ? b->min[1] : b->max[1]) - ray->o[1]) * inv_dir[1];
    if ((tmin > tymax) || (tymin > tmax))
        return 0;
    if (tymin > tmin)
        tmin = tymin;
    if (tymax < tmax)
        tmax = tymax;
    float tzmin = ((dir_is_neg[2] ? b->max[2] : b->min[2]) - ray->o[2]) * inv_dir[2];
    float tzmax = ((dir_is_neg[2] ? b->min[2] : b->max[2]) - ray->o[2]) * inv_dir[2];
    if ((tmin > tzmax) || (tzmin > tmax))
        return 0;
    if (tzmin > tmin)
        tmin = tzmin;
    if (tzmax < tmax)
        tmax = tzmax;
    return (tmin < ray->t_max) && (tmax > 0);
}

int raxel_bvh_accel_intersect(const raxel_bvh_accel *bvh, const raxel_ray *ray) {
    int hit = 0;
    vec3 inv_dir = {1.0f / ray->d[0], 1.0f / ray->d[1], 1.0f / ray->d[2]};
    int dir_is_neg[3] = {inv_dir[0] < 0, inv_dir[1] < 0, inv_dir[2] < 0};

    int to_visit_offset = 0, current_node_index = 0;
    int nodes_to_visit[64];
    while (1) {
        const raxel_linear_bvh_node *node = &bvh->nodes[current_node_index];
        if (raxel_bounds3f_intersect_p(&node->bounds, ray, inv_dir, dir_is_neg)) {
            if (node->n_primitives > 0) {
                // Leaf node hit.
                hit = 1;
                if (to_visit_offset == 0)
                    break;
                current_node_index = nodes_to_visit[--to_visit_offset];
                continue;
            } else {
                // Interior node: traverse children in front-to-back order.
                if (dir_is_neg[node->axis]) {
                    nodes_to_visit[to_visit_offset++] = current_node_index + 1;
                    current_node_index = node->second_child_offset;
                } else {
                    nodes_to_visit[to_visit_offset++] = node->second_child_offset;
                    current_node_index = current_node_index + 1;
                }
                continue;
            }
        } else {
            if (to_visit_offset == 0)
                break;
            current_node_index = nodes_to_visit[--to_visit_offset];
        }
    }
    return hit;
}

void raxel_bvh_accel_destroy(raxel_bvh_accel *bvh, raxel_allocator_t *allocator) {
    if (!bvh) return;
    if (bvh->nodes)
        raxel_free(allocator, bvh->nodes);
    raxel_free(allocator, bvh);
}

// ----------------------------------------------------------------------
// BVH Iterator Implementation
// ----------------------------------------------------------------------
static void *bvh_it_current(raxel_iterator_t *it) {
    raxel_bvh_accel *bvh = (raxel_bvh_accel *)it->__ctx;
    uintptr_t index = (uintptr_t)it->__data;
    if (index >= (uintptr_t)bvh->n_nodes)
        return NULL;
    return &bvh->nodes[index];
}

static void *bvh_it_next(raxel_iterator_t *it) {
    raxel_bvh_accel *bvh = (raxel_bvh_accel *)it->__ctx;
    uintptr_t index = (uintptr_t)it->__data;
    index++;
    it->__data = (void *)index;
    if (index >= (uintptr_t)bvh->n_nodes)
        return NULL;
    return &bvh->nodes[index];
}

raxel_iterator_t raxel_bvh_accel_iterator(raxel_bvh_accel *bvh) {
    raxel_iterator_t it;
    it.__ctx = bvh;
    it.__data = (void *)(uintptr_t)0;
    it.current = bvh_it_current;
    it.next = bvh_it_next;
    return it;
}
