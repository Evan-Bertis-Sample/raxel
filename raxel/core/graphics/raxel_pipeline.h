#ifndef __RAXEL_PIPELINE_H__
#define __RAXEL_PIPELINE_H__

#include <raxel/core/graphics/vk.h>
#include "raxel_surface.h"      // for raxel_surface_t and its callbacks
#include <raxel/core/util.h>

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Enums and structures for pipeline targets.
// -----------------------------------------------------------------------------
typedef enum raxel_pipeline_target_type {
    RAXEL_PIPELINE_TARGET_COLOR = 0,
    RAXEL_PIPELINE_TARGET_DEPTH = 1,
    RAXEL_PIPELINE_TARGET_COUNT
} raxel_pipeline_target_type_t;

typedef struct raxel_pipeline_target {
    raxel_pipeline_target_type_t type;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} raxel_pipeline_target_t;

// Wrap the swapchain data.
typedef struct raxel_pipeline_swapchain {
    VkSwapchainKHR swapchain;
    VkFormat image_format;
    VkExtent2D extent;
    uint32_t image_count;
    // Array of targets for the swapchain images.
    raxel_pipeline_target_t *targets;
} raxel_pipeline_swapchain_t;

// -----------------------------------------------------------------------------
// Pipeline targets: persistent GPU buffers shared between passes.
// -----------------------------------------------------------------------------
typedef struct raxel_pipeline_targets {
    // Internal targets stored as an array indexed by raxel_pipeline_target_type_t.
    raxel_pipeline_target_t internal[RAXEL_PIPELINE_TARGET_COUNT];
    // Which target (by index) should be used for presentation/debugging.
    raxel_pipeline_target_type_t debug_target;
} raxel_pipeline_targets_t;


// -----------------------------------------------------------------------------
// Global pipeline resources: Vulkan objects and other shared resources.
// -----------------------------------------------------------------------------

typedef struct raxel_pipeline_globals {
    raxel_allocator_t allocator;
    VkInstance instance;
    VkPhysicalDevice device_physical;
    VkDevice device;
    VkQueue queue_graphics;
    VkQueue queue_compute;
    uint32_t index_graphics_queue_family;
    uint32_t index_compute_queue_family;
    raxel_surface_t *surface;
    VkCommandPool cmd_pool_graphics;
    VkCommandPool cmd_pool_compute;
    raxel_pipeline_swapchain_t swapchain;
    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkDescriptorPool descriptor_pool;
    raxel_pipeline_targets_t targets;
} raxel_pipeline_globals_t;


// -----------------------------------------------------------------------------
// Base pass structures.
// -----------------------------------------------------------------------------
typedef struct raxel_pipeline_pass_resources {
    VkCommandBuffer command_buffer;
} raxel_pipeline_pass_resources_t;

typedef struct raxel_pipeline_pass {
    raxel_string_t name;
    raxel_pipeline_pass_resources_t resources;
    raxel_allocator_t allocator;
    void *pass_data;
    void (*initialize)(struct raxel_pipeline_pass *pass, raxel_pipeline_globals_t *globals);
    void (*on_begin)(struct raxel_pipeline_pass *pass, raxel_pipeline_globals_t *globals);
    void (*on_end)(struct raxel_pipeline_pass *pass, raxel_pipeline_globals_t *globals);
} raxel_pipeline_pass_t;

// -----------------------------------------------------------------------------
// Pipeline abstraction: A list of passes, global resources, and targets.
// -----------------------------------------------------------------------------
typedef struct raxel_pipeline {
    raxel_pipeline_globals_t resources;
    raxel_list(raxel_pipeline_pass_t) passes;
} raxel_pipeline_t;

// -----------------------------------------------------------------------------
// Global API for the pipeline.
// -----------------------------------------------------------------------------

raxel_pipeline_t *raxel_pipeline_create(raxel_allocator_t *allocator, raxel_surface_t *surface);
void raxel_pipeline_destroy(raxel_pipeline_t *pipeline);
void raxel_pipeline_add_pass(raxel_pipeline_t *pipeline, raxel_pipeline_pass_t pass);
raxel_size_t raxel_pipeline_num_passes(raxel_pipeline_t *pipeline);
raxel_pipeline_pass_t *raxel_pipeline_get_pass(raxel_pipeline_t *pipeline, size_t index);
raxel_pipeline_pass_t *raxel_pipeline_get_pass_by_name(raxel_pipeline_t *pipeline, raxel_string_t name);
int raxel_pipeline_initialize(raxel_pipeline_t *pipeline);
void raxel_pipeline_set_debug_target(raxel_pipeline_t *pipeline, raxel_pipeline_target_type_t target);
void raxel_pipeline_present(raxel_pipeline_t *pipeline);
void raxel_pipeline_run(raxel_pipeline_t *pipeline);
void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif

#endif  // __RAXEL_PIPELINE_H__
