#ifndef __RAXEL_PIPELINE_H__
#define __RAXEL_PIPELINE_H__

#include <vulkan/vulkan.h>

#include "raxel_container.h"  // for raxel_list macros
#include "raxel_core/util.h"  // for raxel_allocator_t, raxel_string_t, VK_CHECK, etc.
#include "raxel_surface.h"    // for raxel_surface_t and its callbacks

#ifdef __cplusplus
extern "C" {
#endif

typedef enum raxel_pipeline_target_type {
    RAXEL_PIPELINE_TARGET_COLOR = 0,
    RAXEL_PIPELINE_TARGET_DEPTH = 1,
    RAXEL_PIPELINE_TARGET_SWAPCHAIN = 2
} raxel_pipeline_target_type_t;

typedef struct raxel_pipeline_target {
    raxel_pipeline_target_type_t type;
    VkImage image;
    VkDeviceMemory memory;
    VkImageView view;
} raxel_pipeline_target_t;

typedef struct raxel_pipeline_swapchain {
    VkSwapchainKHR swapchain;
    VkFormat image_format;
    VkExtent2D extent;
    uint32_t image_count;
    // Wrap all swapchain images together.
    raxel_pipeline_target_t *targets;  // Array of targets (each with type = RAXEL_PIPELINE_TARGET_SWAPCHAIN)
} raxel_pipeline_swapchain_t;

typedef struct raxel_pipeline_globals {
    raxel_allocator_t allocator;
    // Vulkan objects.
    VkInstance instance;
    VkPhysicalDevice device_physical;
    VkDevice device;
    // Queues.
    VkQueue queue_graphics;
    VkQueue queue_compute;
    uint32_t index_graphics_queue_family;
    uint32_t index_compute_queue_family;
    // The surface created via the raxel_surface abstraction.
    raxel_surface_t surface;
    // Command pools.
    VkCommandPool cmd_pool_graphics;
    VkCommandPool cmd_pool_compute;
    // Swapchain-related data is wrapped in a separate struct.
    raxel_pipeline_swapchain_t swapchain;
} raxel_pipeline_globals;

typedef struct raxel_pipeline_targets {
    // Internal targets for rendering.
    raxel_pipeline_target_t internal[2];  // 0: color, 1: depth.
    // Which internal target should be used for presentation/debugging.
    raxel_pipeline_target_type_t debug_target;
} raxel_pipeline_targets;

typedef struct raxel_pipeline_pass_resources {
    // Generic resources for the pass that the pipeline manages.
    VkCommandBuffer command_buffer;
} raxel_pipeline_pass_resources_t;

typedef struct raxel_pipeline_pass {
    raxel_string_t name;
    raxel_pipeline_pass_resources_t resources;
    // Opaque pointer for pass‑specific data.
    void *pass_data;
    // Callback invoked at the beginning of the pass (record commands, etc.)
    void (*on_begin)(struct raxel_pipeline_pass *pass, raxel_pipeline_globals *globals);
    // Callback invoked at the end of the pass (submit, synchronize, etc.)
    void (*on_end)(struct raxel_pipeline_pass *pass, raxel_pipeline_globals *globals);
} raxel_pipeline_pass_t;

typedef struct raxel_pipeline {
    raxel_pipeline_globals resources;
    raxel_pipeline_targets targets;
    raxel_list(raxel_pipeline_pass_t) passes;
    // The surface abstraction used for windowing and input.
    raxel_surface_t surface;
} raxel_pipeline_t;

raxel_pipeline_t *raxel_pipeline_create(raxel_allocator_t *allocator, raxel_surface_t surface);
void raxel_pipeline_destroy(raxel_pipeline_t *pipeline);

inline void raxel_pipeline_add_pass(raxel_pipeline_t *pipeline, raxel_pipeline_pass_t pass);
inline raxel_size_t raxel_pipeline_num_passes(raxel_pipeline_t *pipeline);
inline raxel_pipeline_pass_t *raxel_pipeline_get_pass(raxel_pipeline_t *pipeline, size_t index);
inline raxel_pipeline_pass_t *raxel_pipeline_get_pass_by_name(raxel_pipeline_t *pipeline, raxel_string_t name);

int raxel_pipeline_initialize(raxel_pipeline_t *pipeline);

int raxel_pipeline_create_swapchain(raxel_pipeline_t *pipeline, int width, int height);
void raxel_pipeline_destroy_swapchain(raxel_pipeline_t *pipeline);
int raxel_pipeline_create_targets(raxel_pipeline_t *pipeline, int width, int height);
void raxel_pipeline_destroy_targets(raxel_pipeline_t *pipeline);

void raxel_pipeline_set_debug_target(raxel_pipeline_t *pipeline, raxel_pipeline_target_type_t target);

void raxel_pipeline_present(raxel_pipeline_t *pipeline);
void raxel_pipeline_run(raxel_pipeline_t *pipeline);

void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif

#endif  // __RAXEL_PIPELINE_H__
