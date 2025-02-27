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

// -----------------------------------------------------------------------------
// Internal helper functions (static, snake_case, __-prefixed)
// -----------------------------------------------------------------------------

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
    globals->physicalDevice = devices[0];
    free(devices);
}

// Create a logical device and retrieve graphics and compute queues.
static void __create_logical_device(raxel_pipeline_globals *globals) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(globals->physicalDevice, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(globals->physicalDevice, &queue_family_count, queue_families);
    globals->graphicsQueueFamilyIndex = 0;
    globals->computeQueueFamilyIndex = 0;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            globals->graphicsQueueFamilyIndex = i;
            globals->computeQueueFamilyIndex = i;
            break;
        }
    }
    free(queue_families);

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {0};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = globals->graphicsQueueFamilyIndex;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    VkDeviceCreateInfo device_info = {0};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    // (Device extensions are omitted for brevity.)
    VK_CHECK(vkCreateDevice(globals->physicalDevice, &device_info, NULL, &globals->device));

    vkGetDeviceQueue(globals->device, globals->graphicsQueueFamilyIndex, 0, &globals->graphicsQueue);
    vkGetDeviceQueue(globals->device, globals->computeQueueFamilyIndex, 0, &globals->computeQueue);
}

// Create command pools for graphics and compute.
static void __create_command_pools(raxel_pipeline_globals *globals) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = globals->graphicsQueueFamilyIndex;
    VK_CHECK(vkCreateCommandPool(globals->device, &pool_info, NULL, &globals->graphicsCommandPool));

    pool_info.queueFamilyIndex = globals->computeQueueFamilyIndex;
    VK_CHECK(vkCreateCommandPool(globals->device, &pool_info, NULL, &globals->computeCommandPool));
}

// -----------------------------------------------------------------------------
// Global API implementation
// -----------------------------------------------------------------------------

int raxel_pipeline_initialize(raxel_pipeline_t *pipeline, const char *surface_title, int width, int height) {
    // Initialize the allocator.
    pipeline->resources.allocator = raxel_default_allocator();

    // Initialize Vulkan: create instance, physical device, logical device, and command pools.
    __create_instance(&pipeline->resources);
    __pick_physical_device(&pipeline->resources);
    __create_logical_device(&pipeline->resources);
    __create_command_pools(&pipeline->resources);

    // Create the surface abstraction.
    // raxel_surface_create returns a raxel_surface_t that contains a GLFW window and a VkSurfaceKHR.
    pipeline->surface = raxel_surface_create(surface_title, width, height, pipeline->resources.instance);
    // Also store the surface in globals.
    pipeline->resources.surface = pipeline->surface;

    // Initialize the passes list (using your container API).
    pipeline->passes = raxel_list_create(raxel_pipeline_pass_t, &pipeline->resources.allocator, 0);

    return 0;
}

void raxel_pipeline_run(raxel_pipeline_t *pipeline) {
    // Main loop: poll window events and run each pass's callbacks.
    while (!glfwWindowShouldClose(pipeline->surface.context->window)) {
        glfwPollEvents();

        // Update the surface; this may trigger callbacks for input, resize, etc.
        if (raxel_surface_update(&pipeline->surface) != 0) {
            break;
        }

        size_t pass_count = raxel_list_size(pipeline->passes);
        for (size_t i = 0; i < pass_count; i++) {
            raxel_pipeline_pass_t *pass = &pipeline->passes[i];
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
    raxel_array_destroy(&pipeline->resources.allocator, pipeline->passes);

    // Destroy command pools.
    if (pipeline->resources.computeCommandPool)
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.computeCommandPool, NULL);
    if (pipeline->resources.graphicsCommandPool)
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.graphicsCommandPool, NULL);

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
