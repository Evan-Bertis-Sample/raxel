#ifndef __RAXEL_BVH_H__
#define __RAXEL_BVH_H__

#include <stdint.h>
#include <cglm/cglm.h>

typedef struct raxel_bounds {
    vec3 min;
    vec3 max;
} raxel_bounds_t;

raxel_bounds_t raxel_bounds_union(raxel_bounds_t a, raxel_bounds_t b);
raxel_bounds_t raxel_bounds_union_point(raxel_bounds_t a, vec3 point);
int raxel_bounds_contains(raxel_bounds_t a, raxel_bounds_t b);
int raxel_bounds_contains_point(raxel_bounds_t a, vec3 point);

typedef struct raxel_bvh_node {
    raxel_bounds_t bounds;
    struct raxel_bvh_node *left;
    struct raxel_bvh_node *right;
    uint32_t start;
    uint32_t end;
} raxel_bvh_node_t;

typedef struct raxel_bvh {
    raxel_bvh_node_t *nodes;
    uint32_t node_count;
} raxel_bvh_t;


#endif // __RAXEL_BVH_H__