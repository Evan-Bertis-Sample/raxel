#ifndef __RAXEL_PIPELINE_H__
#define __RAXEL_PIPELINE_H__

#include <vulkan.h>

#include "<raxel/core/util.h>"  // for raxel_allocator_t, raxel_string_t, VK_CHECK, etc.
#include "raxel_container.h"    // for raxel_list macros
#include "raxel_surface.h"      // for raxel_surface_t and its callbacks

#ifdef __cplusplus
extern "C" {
#endif

// ----------------------------------------------------------------------------
// Global pipeline resources: Vulkan objects and other shared resources.
// ----------------------------------------------------------------------------
typedef struct raxel_pipeline_globals {
    raxel_allocator_t allocator;

    // Vulkan objects.
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    VkQueue computeQueue;
    uint32_t graphicsQueueFamilyIndex;
    uint32_t computeQueueFamilyIndex;

    // The Vulkan surface created from the GLFW window.
    raxel_surface_t surface;

    // Command pools.
    VkCommandPool graphicsCommandPool;
    VkCommandPool computeCommandPool;

    // (Other global Vulkan objects such as swapchain, descriptor pools, etc.)
} raxel_pipeline_globals;

// ----------------------------------------------------------------------------
// Base pass structures.
// ----------------------------------------------------------------------------
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

// ----------------------------------------------------------------------------
// Pipeline abstraction: A list of passes and global resources.
// ----------------------------------------------------------------------------
typedef struct raxel_pipeline {
    raxel_pipeline_globals resources;
    raxel_list(raxel_pipeline_pass_t) passes;
    // The surface abstraction used for windowing and input.
    raxel_surface_t surface;
} raxel_pipeline_t;

// ----------------------------------------------------------------------------
// Global API for the pipeline.
// ----------------------------------------------------------------------------

/**
 * Initialize the pipeline.
 *
 * This function will:
 *   - Initialize Vulkan (instance, physical device, logical device, queues).
 *   - Create command pools.
 *   - Create the surface using the raxel_surface abstraction.
 *     (The raxel_surface_t is kept in the pipeline; the Vulkan surface is
 *      stored in the globals.)
 *
 * @param pipeline Pointer to a pipeline object to initialize.
 * @param surfaceTitle Title of the window/surface.
 * @param width, height Dimensions for the surface.
 * @return 0 on success, non-zero on failure.
 */
int raxel_pipeline_initialize(raxel_pipeline_t *pipeline, const char *surfaceTitle, int width, int height);

/**
 * Run the pipeline.
 *
 * This function polls the surface events (via raxel_surface_update) and
 * iterates over the list of passes calling their callbacks.
 *
 * @param pipeline Pointer to an initialized pipeline.
 */
void raxel_pipeline_run(raxel_pipeline_t *pipeline);

/**
 * Clean up and destroy the pipeline and all associated Vulkan objects.
 *
 * @param pipeline Pointer to the pipeline to clean up.
 */
void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline);

#ifdef __cplusplus
}
#endif

#endif  // __RAXEL_PIPELINE_H__
