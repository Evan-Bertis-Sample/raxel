#ifndef __RAXEL_PIPELINE_H__
#define __RAXEL_PIPELINE_H__

#include <vulkan.h>

#include "<raxel/core/util.h>"  // for raxel_allocator_t, raxel_string_t, VK_CHECK, etc.
#include "raxel_container.h"    // for raxel_list macros
#include "raxel_surface.h"      // for raxel_surface_t and its callbacks

#ifdef __cplusplus
extern "C" {
#endif

typedef struct raxel_pipeline_globals {
    raxel_allocator_t allocator;
    // Vulkan resources
    VkInstance instance;
    VkPhysicalDevice device_physical;
    VkDevice device;

    // Queues
    VkQueue queue_graphics;
    VkQueue queue_compute;
    uint32_t index_graphics_queue_family;
    uint32_t index_compute_queue_family;

    // command pools
    VkCommandPool cmd_pool_grpahics;
    VkCommandPool cmd_pool_compute;

} raxel_pipeline_globals;


typedef struct raxel_pipeline_pass_resources {
    // Generic resources for the pass that the pipeline manages.
    VkCommandBuffer commandBuffer;  // For example, a command buffer allocated from a pool.
} raxel_pipeline_pass_resources_t;

typedef struct raxel_pipeline_pass {
    raxel_string_t name;
    raxel_pipeline_pass_resources_t resources;
    // Opaque pointer for pass‑specific data.
    void *pass_data;
    // Callback invoked at the beginning of the pass (record commands, etc.)
    void (*on_begin)(struct raxel_pipeline_pass *pass);
    // Callback invoked at the end of the pass (submit, synchronize, etc.)
    void (*on_end)(struct raxel_pipeline_pass *pass);
} raxel_pipeline_pass_t;


typedef struct raxel_pipeline {
    raxel_pipeline_globals resources;
    raxel_list(raxel_pipeline_pass_t) __passes;
    // The surface abstraction used for windowing and input.
    raxel_surface_t surface;
} raxel_pipeline_t;


raxel_pipeline_t *raxel_pipeline_create(raxel_allocator_t *allocator, raxel_surface_t surface);
void raxel_pipeline_destroy(raxel_pipeline_t *pipeline);

inline void raxel_pipeline_add_pass(raxel_pipeline_t *pipeline, raxel_pipeline_pass_t pass);
inline raxel_size_t raxel_pipeline_num_passes(raxel_pipeline_t *pipeline);
inline raxel_pipeline_pass_t *raxel_pipeline_get_pass_by_name(raxel_pipeline_t *pipeline, raxel_string_t name);
inline raxel_pipeline_pass_t *raxel_pipeline_get_pass(raxel_pipeline_t *pipeline, size_t index);

int raxel_pipeline_initialize(raxel_pipeline_t *pipeline);
void raxel_pipeline_run(raxel_pipeline_t *pipeline);
void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif

#endif  // __RAXEL_PIPELINE_H__
