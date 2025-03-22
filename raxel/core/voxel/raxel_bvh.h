#ifndef __RAXEL_BVH_H__
#define __RAXEL_BVH_H__

#include <stdint.h>
#include <raxel/core/util.h>
#include <cglm/cglm.h>  // using cglm for math types

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------
// Bounding-box type (using cglm’s vec3)
// ----------------------------------------------------------------------
typedef struct raxel_bvh_bounds {
    vec3 min;
    vec3 max;
} raxel_bvh_bounds_t;

// ----------------------------------------------------------------------
// BVH Build Node (temporary tree structure)
// ----------------------------------------------------------------------
typedef struct raxel_bvh_build_node_t {
    raxel_bvh_bounds_t bounds;
    int first_prim_offset;   // valid for leaf nodes
    int n_primitives;        // >0 for leaf, 0 for interior
    int split_axis;          // splitting axis: 0=x, 1=y, 2=z
    struct raxel_bvh_build_node_t *children[2];
} raxel_bvh_build_node_t;

// ----------------------------------------------------------------------
// Linear BVH Node (final, compact representation; 32 bytes each)
// ----------------------------------------------------------------------
typedef struct raxel_linear_bvh_node_t {
    raxel_bvh_bounds_t bounds;
    union {
        int primitives_offset;    // for leaf nodes
        int second_child_offset;   // for interior nodes
    };
    uint16_t n_primitives;  // 0 means interior node
    uint8_t axis;           // interior node: splitting axis
    uint8_t pad[1];         // padding to 32 bytes
} raxel_linear_bvh_node_t;

// ----------------------------------------------------------------------
// BVH Accelerator structure
// ----------------------------------------------------------------------
typedef struct raxel_bvh_accel_t {
    raxel_linear_bvh_node_t *nodes;  // linear array of nodes
    int n_nodes;                   // total number of nodes
    int max_leaf_size;             // maximum primitives per leaf
} raxel_bvh_accel_t;

// ----------------------------------------------------------------------
// BVH Public API
// ----------------------------------------------------------------------

/**
 * raxel_bvh_accel_build constructs a BVH from an array of primitive bounds
 * and an array of primitive indices. The BVH is built with a maximum of
 * max_leaf_size primitives per leaf.
 */
raxel_bvh_accel_t *raxel_bvh_accel_build(raxel_bvh_bounds_t *primitive_bounds,
                                       int *primitive_indices,
                                       int n,
                                       int max_leaf_size,
                                       raxel_allocator_t *allocator);

/**
 * raxel_bvh_accel_destroy frees all memory associated with the BVH.
 */
void raxel_bvh_accel_destroy(raxel_bvh_accel_t *bvh, raxel_allocator_t *allocator);

/**
 * raxel_bvh_accel_iterator returns an iterator over the BVH’s linear nodes.
 * The iterator traverses the nodes in linear (depth-first) order.
 */
raxel_iterator_t raxel_bvh_accel_iterator(raxel_bvh_accel_t *bvh);

/**
 * raxel_bvh_accel_build_from_voxel_world constructs a BVH accelerator
 * directly from a raxel_voxel_world. It gathers all non-empty voxels,
 * computes their bounding boxes (each voxel is [x, x+1], [y, y+1], [z, z+1]),
 * and builds a BVH with a maximum of max_leaf_size primitives per leaf.
 */
raxel_bvh_accel_t *raxel_bvh_accel_build_from_voxel_world(struct raxel_voxel_world *world,
                                                        int max_leaf_size,
                                                        raxel_allocator_t *allocator);

#ifdef __cplusplus
}
#endif

#endif  // __RAXEL_BVH_H__
