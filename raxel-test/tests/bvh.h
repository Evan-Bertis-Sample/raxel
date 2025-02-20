// raxel_bvh_tests.c

#include <raxel/core/util.h>
#include <raxel/core/graphics.h>
#include <stdio.h>
#include <cglm/cglm.h>

// Test: Build a BVH from a small set of primitives.
RAXEL_TEST(test_bvh_build) {
    raxel_allocator_t allocator = raxel_default_allocator();
    int n = 4;
    raxel_bounds3f primitive_bounds[4];
    int primitive_indices[4];
    
    // Create 4 primitives. Each primitive is a small box centered at (i,i,i) with half-extent 0.5.
    for (int i = 0; i < n; i++) {
        primitive_indices[i] = i;
        vec3 center = { (float)i, (float)i, (float)i };
        vec3 delta  = { 0.5f, 0.5f, 0.5f };
        glm_vec3_sub(center, delta, primitive_bounds[i].min);
        glm_vec3_add(center, delta, primitive_bounds[i].max);
    }
    
    raxel_bvh_accel *bvh = raxel_bvh_accel_build(primitive_bounds, primitive_indices, n, 2, &allocator);
    RAXEL_TEST_ASSERT(bvh != NULL);
    RAXEL_TEST_ASSERT(bvh->n_nodes > 0);
    
    raxel_bvh_accel_destroy(bvh, &allocator);
}

// Test: BVH intersection (hit case)
RAXEL_TEST(test_bvh_intersect_hit) {
    raxel_allocator_t allocator = raxel_default_allocator();
    int n = 4;
    raxel_bounds3f primitive_bounds[4];
    int primitive_indices[4];
    
    for (int i = 0; i < n; i++) {
        primitive_indices[i] = i;
        vec3 center = { (float)i, (float)i, (float)i };
        vec3 delta  = { 0.5f, 0.5f, 0.5f };
        glm_vec3_sub(center, delta, primitive_bounds[i].min);
        glm_vec3_add(center, delta, primitive_bounds[i].max);
    }
    
    raxel_bvh_accel *bvh = raxel_bvh_accel_build(primitive_bounds, primitive_indices, n, 2, &allocator);
    
    // Create a ray that starts at (-1,-1,-1) and goes in (1,1,1) direction.
    raxel_ray ray;
    ray.o[0] = ray.o[1] = ray.o[2] = -1.0f;
    ray.d[0] = ray.d[1] = ray.d[2] = 1.0f;
    ray.t_max = 1000.0f;
    
    int hit = raxel_bvh_accel_intersect(bvh, &ray);
    RAXEL_TEST_ASSERT(hit == 1);
    
    raxel_bvh_accel_destroy(bvh, &allocator);
}

// Test: BVH intersection (miss case)
RAXEL_TEST(test_bvh_intersect_miss) {
    raxel_allocator_t allocator = raxel_default_allocator();
    int n = 4;
    raxel_bounds3f primitive_bounds[4];
    int primitive_indices[4];
    
    for (int i = 0; i < n; i++) {
        primitive_indices[i] = i;
        vec3 center = { (float)i, (float)i, (float)i };
        vec3 delta  = { 0.5f, 0.5f, 0.5f };
        glm_vec3_sub(center, delta, primitive_bounds[i].min);
        glm_vec3_add(center, delta, primitive_bounds[i].max);
    }
    
    raxel_bvh_accel *bvh = raxel_bvh_accel_build(primitive_bounds, primitive_indices, n, 2, &allocator);
    
    // Create a ray that starts far away and goes in a direction that misses the primitives.
    raxel_ray ray;
    ray.o[0] = ray.o[1] = ray.o[2] = -10.0f;
    ray.d[0] = ray.d[1] = ray.d[2] = -1.0f;
    ray.t_max = 1000.0f;
    
    int hit = raxel_bvh_accel_intersect(bvh, &ray);
    RAXEL_TEST_ASSERT(hit == 0);
    
    raxel_bvh_accel_destroy(bvh, &allocator);
}

// Test: Iterator over the BVH's linear nodes.
RAXEL_TEST(test_bvh_iterator) {
    raxel_allocator_t allocator = raxel_default_allocator();
    int n = 4;
    raxel_bounds3f primitive_bounds[4];
    int primitive_indices[4];
    
    for (int i = 0; i < n; i++) {
        primitive_indices[i] = i;
        vec3 center = { (float)i, (float)i, (float)i };
        vec3 delta  = { 0.5f, 0.5f, 0.5f };
        glm_vec3_sub(center, delta, primitive_bounds[i].min);
        glm_vec3_add(center, delta, primitive_bounds[i].max);
    }
    
    raxel_bvh_accel *bvh = raxel_bvh_accel_build(primitive_bounds, primitive_indices, n, 2, &allocator);
    raxel_iterator_t it = raxel_bvh_accel_iterator(bvh);
    int count = 0;
    while (1) {
        raxel_linear_bvh_node *node = (raxel_linear_bvh_node *)it.current(&it);
        if (!node)
            break;
        count++;
        it.next(&it);
    }
    RAXEL_TEST_ASSERT_EQUAL_INT(count, bvh->n_nodes);
    
    raxel_bvh_accel_destroy(bvh, &allocator);
}

void register_bvh_tests(void) {
    RAXEL_TEST_REGISTER(test_bvh_build);
    RAXEL_TEST_REGISTER(test_bvh_intersect_hit);
    RAXEL_TEST_REGISTER(test_bvh_intersect_miss);
    RAXEL_TEST_REGISTER(test_bvh_iterator);
}
