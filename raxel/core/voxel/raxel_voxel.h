#ifndef __RAXEL_VOXEL_H__
#define __RAXEL_VOXEL_H__

#include <cglm/cglm.h>
#include <raxel/core/graphics.h>                      // for raxel_compute_shader_t, raxel_pipeline_t
#include <raxel/core/graphics/passes/compute_pass.h>  // for raxel_compute_shader_t
#include <raxel/core/util.h>                          // for raxel_allocator_t, raxel_string_t, etc.
#include <stdint.h>

typedef uint32_t raxel_material_handle_t;
typedef int32_t raxel_coord_t;

typedef struct raxel_voxel {
    raxel_material_handle_t material;
} raxel_voxel_t;

#define RAXEL_VOXEL_CHUNK_SIZE 32
#define RAXEL_MAX_LOADED_CHUNKS 32
#define RAXEL_BVH_MAX_NODES 4096
#define MAX_LEAF_SIZE_BVH 32

typedef enum raxel_voxel_chunk_state {
    RAXEL_VOXEL_CHUNK_STATE_COUNT = 0,  // not used atm
} raxel_voxel_chunk_state_t;

typedef struct raxel_voxel_chunk_meta {
    // defines the bottom-left corner of the chunk
    raxel_coord_t x;
    raxel_coord_t y;
    raxel_coord_t z;
    raxel_voxel_chunk_state_t state;
} raxel_voxel_chunk_meta_t;

typedef struct raxel_voxel_chunk {
    // all of these voxel's coordinates are relative to the chunk's bottom-left corner, and implicitly
    // stored in the index of the array
    raxel_voxel_t voxels[RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE * RAXEL_VOXEL_CHUNK_SIZE];
} raxel_voxel_chunk_t;

typedef struct raxel_voxel_material_attributes {
    vec4 color;
} raxel_voxel_material_attributes_t;

typedef struct raxel_voxel_material {
    raxel_string_t name;
    raxel_voxel_material_attributes_t attributes;
} raxel_voxel_material_t;

// This describes the options for updating the voxel world -- ie loading/unloading chunks, etc.
typedef struct raxel_voxel_world_update_options {
    float view_distance;
    vec3 camera_position;
    vec3 camera_direction;
} raxel_voxel_world_update_options_t;

typedef struct raxel_voxel_world {
    raxel_list(raxel_voxel_chunk_meta_t) chunk_meta;  // index of chunk in chunks
    raxel_list(raxel_voxel_chunk_t) chunks;           // the first __num_loaded_chunks are loaded
    raxel_size_t __num_loaded_chunks;                 // between 0 and RAXEL_MAX_LOADED_CHUNKS
    raxel_allocator_t *allocator;
    raxel_list(raxel_voxel_material_t) materials;
    raxel_voxel_world_update_options_t prev_update_options;
} raxel_voxel_world_t;

raxel_voxel_world_t *raxel_voxel_world_create(raxel_allocator_t *allocator);
void raxel_voxel_world_destroy(raxel_voxel_world_t *world);
void raxel_voxel_world_add_material(raxel_voxel_world_t *world, raxel_string_t name, raxel_voxel_material_attributes_t attributes);
raxel_material_handle_t raxel_voxel_world_get_material_handle(raxel_voxel_world_t *world, raxel_string_t name);

raxel_voxel_chunk_t *raxel_voxel_world_get_chunk(raxel_voxel_world_t *world, raxel_coord_t x, raxel_coord_t y, raxel_coord_t z);
raxel_voxel_t raxel_voxel_world_get_voxel(raxel_voxel_world_t *world, raxel_coord_t x, raxel_coord_t y, raxel_coord_t z);
void raxel_voxel_world_place_voxel(raxel_voxel_world_t *world, raxel_coord_t x, raxel_coord_t y, raxel_coord_t z, raxel_voxel_t voxel);

void raxel_voxel_world_update(raxel_voxel_world_t *world, raxel_voxel_world_update_options_t *options, raxel_compute_shader_t *compute_shader, raxel_pipeline_t *pipeline);

void raxel_voxel_world_set_sb(raxel_voxel_world_t *world, raxel_compute_shader_t *compute_shader, raxel_pipeline_t *pipeline);

typedef struct raxel_bvh_bounds {
    vec3 min;
    vec3 max;
} raxel_bvh_bounds_t;

typedef struct __raxel_bvh_build_node {
    raxel_bvh_bounds_t bounds;
    int first_prim_offset;  // valid for leaf nodes
    int n_primitives;       // >0 for leaf, 0 for interior
    int split_axis;         // splitting axis: 0=x, 1=y, 2=z
    struct __raxel_bvh_build_node *children[2];
} __raxel_bvh_build_node_t;

typedef struct raxel_linear_bvh_node_t {
    raxel_bvh_bounds_t bounds;
    union {
        int32_t primitives_offset;    // for leaf nodes
        int32_t second_child_offset;  // for interior nodes
    };
    uint32_t n_primitives;  // 0 means interior node
    uint32_t axis;           // interior node: splitting axis
} raxel_linear_bvh_node_t;

typedef struct raxel_bvh_accel_t {
    raxel_linear_bvh_node_t nodes[RAXEL_BVH_MAX_NODES];  // linear array of nodes
    int32_t n_nodes;                                         // total number of nodes
    int32_t max_leaf_size;                                   // maximum primitives per leaf
} raxel_bvh_accel_t;

raxel_bvh_accel_t *raxel_bvh_accel_build(raxel_bvh_bounds_t *primitive_bounds,
                                         int *primitive_indices,
                                         int n,
                                         int max_leaf_size,
                                         raxel_allocator_t *allocator);

void raxel_bvh_accel_destroy(raxel_bvh_accel_t *bvh, raxel_allocator_t *allocator);

raxel_bvh_accel_t *raxel_bvh_accel_build_from_voxel_world(raxel_voxel_world_t *world,
                                                          int max_leaf_size,
                                                          raxel_allocator_t *allocator);
                                                          
void raxel_bvh_accel_print(raxel_bvh_accel_t *bvh);

typedef struct __raxel_voxel_world_gpu {
    raxel_bvh_accel_t bvh;
    uint32_t num_loaded_chunks;
    raxel_voxel_chunk_meta_t chunk_meta[RAXEL_MAX_LOADED_CHUNKS];
    raxel_voxel_chunk_t chunks[RAXEL_MAX_LOADED_CHUNKS];
} __raxel_voxel_world_gpu_t;

#endif  // __RAXEL_VOXEL_H__
