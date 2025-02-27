#include "raxel_pipeline.h"

#include <GLFW/glfw3.h>
#include <raxel/core/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef VK_CHECK
#define VK_CHECK(x)                                     \
    do {                                                \
        VkResult err = x;                               \
        if (err != VK_SUCCESS) {                        \
            fprintf(stderr, "Vulkan error: %d\n", err); \
            exit(EXIT_FAILURE);                         \
        }                                               \
    } while (0)
#endif

/**------------------------------------------------------------------------
 *                           Utility Functions
 *------------------------------------------------------------------------**/

// Create a Vulkan instance.
static void __create_instance(raxel_pipeline_globals *globals) {
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "raxel_pipeline_renderer";
    app_info.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    // (For brevity, extensions and validation layers are omitted.)
    VK_CHECK(vkCreateInstance(&create_info, NULL, &globals->instance));
}

// Pick the first available physical device.
static void __pick_physical_device(raxel_pipeline_globals *globals) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(globals->instance, &device_count, NULL);
    if (device_count == 0) {
        fprintf(stderr, "No Vulkan-compatible GPU found!\n");
        exit(EXIT_FAILURE);
    }
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * device_count);
    vkEnumeratePhysicalDevices(globals->instance, &device_count, devices);
    globals->device_physical = devices[0];
    free(devices);
}

// Create a logical device and retrieve graphics and compute queues.
static void __create_logical_device(raxel_pipeline_globals *globals) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(globals->device_physical, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(globals->device_physical, &queue_family_count, queue_families);
    globals->index_graphics_queue_family = 0;
    globals->index_compute_queue_family = 0;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            globals->index_graphics_queue_family = i;
            globals->index_compute_queue_family = i;
            break;
        }
    }
    free(queue_families);

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {0};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = globals->index_graphics_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_info = {0};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    // (Device extensions are omitted for brevity.)
    VK_CHECK(vkCreateDevice(globals->device_physical, &device_info, NULL, &globals->device));

    vkGetDeviceQueue(globals->device, globals->index_graphics_queue_family, 0, &globals->queue_graphics);
    vkGetDeviceQueue(globals->device, globals->index_compute_queue_family, 0, &globals->queue_compute);
}

// Create command pools for graphics and compute.
static void __create_command_pools(raxel_pipeline_globals *globals) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = globals->index_graphics_queue_family;
    VK_CHECK(vkCreateCommandPool(globals->device, &pool_info, NULL, &globals->cmd_pool_grpahics));

    pool_info.queueFamilyIndex = globals->index_compute_queue_family;
    VK_CHECK(vkCreateCommandPool(globals->device, &pool_info, NULL, &globals->cmd_pool_compute));
}

/**------------------------------------------------------------------------
 *                           Public API
 *------------------------------------------------------------------------**/

raxel_pipeline_t *raxel_pipeline_create(raxel_acllocator_t *allocator, raxel_surface_t surface) {
    raxel_pipeline_t *pipeline = raxel_malloc(allocator, sizeof(raxel_pipeline_t));
    pipeline->resources.allocator = *allocator;
    pipeline->resources.instance = VK_NULL_HANDLE;
    pipeline->resources.device_physical = VK_NULL_HANDLE;
    pipeline->resources.device = VK_NULL_HANDLE;
    pipeline->resources.queue_graphics = VK_NULL_HANDLE;
    pipeline->resources.queue_compute = VK_NULL_HANDLE;
    pipeline->resources.index_graphics_queue_family = 0;
    pipeline->resources.index_compute_queue_family = 0;
    pipeline->resources.surface = surface;
    pipeline->resources.cmd_pool_grpahics = VK_NULL_HANDLE;
    pipeline->resources.cmd_pool_compute = VK_NULL_HANDLE;
    pipeline->__passes = raxel_list_create(raxel_pipeline_pass_t, allocator, 0);
    return pipeline;
}

void raxel_pipeline_destroy(raxel_pipeline_t *pipeline) {
    raxel_list_destroy(pipeline->resources.allocator, pipeline->__passes);
    raxel_free(pipeline->resources.allocator, pipeline);
}

int raxel_pipeline_initialize(raxel_pipeline_t *pipeline) {
    __create_instance(&pipeline->resources);
    __pick_physical_device(&pipeline->resources);
    __create_logical_device(&pipeline->resources);
    __create_command_pools(&pipeline->resources);
}

void raxel_pipeline_run(raxel_pipeline_t *pipeline) {
    // Main loop: poll window events and run each pass's callbacks.
    while (!glfwWindowShouldClose(pipeline->surface.context->window)) {
        glfwPollEvents();

        // Update the surface; this may trigger callbacks for input, resize, etc.
        if (raxel_surface_update(&pipeline->surface) != 0) {
            break;
        }

        size_t pass_count = raxel_list_size(pipeline->__passes);
        for (size_t i = 0; i < pass_count; i++) {
            raxel_pipeline_pass_t *pass = &pipeline->__passes[i];
            if (pass->on_begin) {
                pass->on_begin(pass);
            }
            if (pass->on_end) {
                pass->on_end(pass);
            }
        }
        // (Presentation and synchronization steps would be added here.)
    }
    vkDeviceWaitIdle(pipeline->resources.device);
}

void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline) {
    // Clean up passes.
    raxel_array_destroy(&pipeline->resources.allocator, pipeline->__passes);

    // Destroy command pools.
    if (pipeline->resources.cmd_pool_compute)
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.cmd_pool_compute, NULL);
    if (pipeline->resources.cmd_pool_grpahics)
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.cmd_pool_grpahics, NULL);

    // Destroy the Vulkan surface stored in the surface abstraction.
    if (pipeline->resources.surface.context && pipeline->resources.surface.context->vk_surface)
        vkDestroySurfaceKHR(pipeline->resources.instance, pipeline->resources.surface.context->vk_surface, NULL);

    // Destroy logical device and instance.
    if (pipeline->resources.device)
        vkDestroyDevice(pipeline->resources.device, NULL);
    if (pipeline->resources.instance)
        vkDestroyInstance(pipeline->resources.instance, NULL);

    // Destroy the surface abstraction.
    raxel_surface_destroy(&pipeline->surface);
    free(pipeline->surface.context);
}
