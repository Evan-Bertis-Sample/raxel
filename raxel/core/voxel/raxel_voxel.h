#ifndef __RAXEL_VOXEL_H__
#define __RAXEL_VOXEL_H__

#include <cglm/cglm.h>
#include <raxel/core/util.h>  // for raxel_allocator_t, raxel_string_t, etc.
#include <stdint.h>

typedef uint8_t raxel_material_handle_t;
typedef int32_t raxel_coord_t;

typedef struct raxel_voxel {
    raxel_material_handle_t material;
} raxel_voxel_t;

#define RAXEL_VOXEL_CHUNK_SIZE 32
#define RAXEL_MAX_LOADED_CHUNKS 16

tyepdef enum raxel_voxel_chunk_state {
    RAXEL_VOXEL_CHUNK_STATE_COUNT = 0, // not used atm
}

typedef struct raxel_voxel_chunk_meta {
    // defines the bottom-left corner of the chunk
    raxel_chunk_coord_t x;
    raxel_chunk_coord_t y;
    raxel_chunk_coord_t z;
    raxel_voxel_chunk_state state;
} raxel_voxel_chunk_meta_t;

typedef struct raxel_voxel_chunk {
    // all of these voxel's coordinates are relative to the chunk's bottom-left corner, and implicitly
    // stored in the index of the array
    raxel_voxel_t voxels[RAXEL_VOXEL_CHUNK_SIZE][RAXEL_VOXEL_CHUNK_SIZE][RAXEL_VOXEL_CHUNK_SIZE];
} raxel_voxel_chunk_t;

typedef struct raxel_voxel_material {
    raxel_string_t name;
    vec4 color;
} raxel_voxel_material_t;

typedef struct raxel_voxel_world {
    raxel_list(raxel_voxel_chunk_t) chunks; // the first __num_loaded_chunks are loaded
    raxel_list(raxel_voxel_chunk_meta_t) chunk_meta; // index of chunk in chunks
    raxel_size_t __num_loaded_chunks; // between 0 and RAXEL_MAX_LOADED_CHUNKS
    raxel_allocator_t allocator;
    raxel_list(raxel_voxel_material_t) materials;
} raxel_voxel_world_t;


// This describes the options for updating the voxel world -- ie loading/unloading chunks, etc.
typedef struct raxel_voxel_world_update_options {
    float view_distance;
    vec3 camera_position;
    vec3 camera_direction;
} raxel_voxel_world_update_options_t;

raxel_voxel_world_t *raxel_voxel_world_create(raxel_allocator_t *allocator);
void raxel_voxel_world_destroy(raxel_voxel_world_t *world);
void raxel_voxel_world_add_material(raxel_voxel_world_t *world, raxel_string_t name, vec4 color);
raxel_material_handle_t raxel_voxel_world_get_material_handle(raxel_voxel_world_t *world, raxel_string_t name);

raxel_voxel_chunk_t *raxel_world_get_chunk(raxel_voxel_world_t *world, raxel_chunk_coord_t x, raxel_chunk_coord_t y, raxel_chunk_coord_t z);
raxel_voxel_t raxel_world_get_voxel(raxel_voxel_world_t *world, raxel_coord_t x, raxel_coord_t y, raxel_coord_t z);
void raxel_world_place_voxel(raxel_voxel_world_t *world, raxel_coord_t x, raxel_coord_t y, raxel_coord_t z, raxel_voxel_t voxel);

void raxel_voxel_world_update(raxel_voxel_world_t *world, raxel_voxel_world_update_options_t *options);


#endif  // __RAXEL_VOXEL_H__
