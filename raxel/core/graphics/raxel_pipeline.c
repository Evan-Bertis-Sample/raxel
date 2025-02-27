#include "raxel_pipeline.h"

#include <raxel/core/util.h>  // For raxel_list_create, raxel_list_destroy
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raxel_surface.h"  // For raxel_surface_create, raxel_surface_update, raxel_surface_destroy

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

// ----------------------------------------------------------------------------
// Helper functions for Vulkan initialization (simplified)
// ----------------------------------------------------------------------------
static void __create_instance(raxel_pipeline_globals *globals) {
    VkApplicationInfo appInfo = {0};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RAXEL Pipeline Renderer";
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo instInfo = {0};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
    // (Extensions and validation layers omitted for brevity.)
    VK_CHECK(vkCreateInstance(&instInfo, NULL, &globals->instance));
}

static void __pick_physical_device(raxel_pipeline_globals *globals) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(globals->instance, &deviceCount, NULL);
    if (deviceCount == 0) {
        fprintf(stderr, "No Vulkan-compatible GPU found!\n");
        exit(EXIT_FAILURE);
    }
    VkPhysicalDevice *devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(globals->instance, &deviceCount, devices);
    globals->physicalDevice = devices[0];  // Simply choose the first device.
    free(devices);
}

static void __create_logical_device(raxel_pipeline_globals *globals) {
    // Query queue families.
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(globals->physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties *queueFamilies = malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(globals->physicalDevice, &queueFamilyCount, queueFamilies);
    globals->graphicsQueueFamilyIndex = 0;
    globals->computeQueueFamilyIndex = 0;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            globals->graphicsQueueFamilyIndex = i;
            globals->computeQueueFamilyIndex = i;
            break;
        }
    }
    free(queueFamilies);

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {0};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = globals->graphicsQueueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    VkDeviceCreateInfo deviceInfo = {0};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    // (Device extensions omitted for brevity.)
    VK_CHECK(vkCreateDevice(globals->physicalDevice, &deviceInfo, NULL, &globals->device));

    vkGetDeviceQueue(globals->device, globals->graphicsQueueFamilyIndex, 0, &globals->graphicsQueue);
    vkGetDeviceQueue(globals->device, globals->computeQueueFamilyIndex, 0, &globals->computeQueue);
}

static void __create_cmd_pools(raxel_pipeline_globals *globals) {
    VkCommandPoolCreateInfo poolInfo = {0};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = globals->graphicsQueueFamilyIndex;
    VK_CHECK(vkCreateCommandPool(globals->device, &poolInfo, NULL, &globals->graphicsCommandPool));

    poolInfo.queueFamilyIndex = globals->computeQueueFamilyIndex;
    VK_CHECK(vkCreateCommandPool(globals->device, &poolInfo, NULL, &globals->computeCommandPool));
}

// ----------------------------------------------------------------------------
// Pipeline API implementations.
// ----------------------------------------------------------------------------

int raxel_pipeline_initialize(raxel_pipeline_t *pipeline, const char *surfaceTitle, int width, int height) {
    // Initialize our allocator.
    pipeline->resources.allocator = raxel_default_allocator();

    // Initialize Vulkan.
    __create_instance(&pipeline->resources);
    __pick_physical_device(&pipeline->resources);
    __create_logical_device(&pipeline->resources);
    __create_cmd_pools(&pipeline->resources);

    // Create the surface using the raxel_surface abstraction.
    // We assume a helper function raxel_surface_create exists that creates
    // a GLFW window and returns a raxel_surface_t.
    pipeline->surface = raxel_surface_create(surfaceTitle, width, height);
    // Create a Vulkan surface from the GLFW window.
    VK_CHECK(glfwCreateWindowSurface(pipeline->resources.instance,
                                     pipeline->surface.context->window,
                                     NULL,
                                     &pipeline->resources.surface));

    // Create the passes list.
    pipeline->passes = raxel_list_create(raxel_pipeline_pass_t, &pipeline->resources.allocator, 0);

    return 0;
}

void raxel_pipeline_run(raxel_pipeline_t *pipeline) {
    // Main loop.
    while (!glfwWindowShouldClose(pipeline->surface.context->window)) {
        glfwPollEvents();

        // Update the surface (input, window events, etc.).
        if (raxel_surface_update(&pipeline->surface) != 0) {
            break;
        }

        // For each pass in the pipeline, call its callbacks.
        for (size_t i = 0; i < raxel_list_size(pipeline->passes); i++) {
            raxel_pipeline_pass_t *pass = &pipeline->passes[i];
            if (pass->on_begin) pass->on_begin(pass);
            if (pass->on_end) pass->on_end(pass);
        }

        // (Presentation, swapchain handling, and synchronization would go here.)
    }
    vkDeviceWaitIdle(pipeline->resources.device);
}

void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline) {
    // Clean up passes.
    raxel_array_destroy(&pipeline->resources.allocator, pipeline->passes);

    // Destroy Vulkan command pools.
    if (pipeline->resources.computeCommandPool)
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.computeCommandPool, NULL);
    if (pipeline->resources.graphicsCommandPool)
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.graphicsCommandPool, NULL);

    // Destroy Vulkan surface.
    if (pipeline->resources.surface)
        vkDestroySurfaceKHR(pipeline->resources.instance, pipeline->resources.surface, NULL);

    // Destroy device and instance.
    if (pipeline->resources.device)
        vkDestroyDevice(pipeline->resources.device, NULL);
    if (pipeline->resources.instance)
        vkDestroyInstance(pipeline->resources.instance, NULL);

    // Destroy the surface using the raxel_surface abstraction.
    raxel_surface_destroy(&pipeline->surface);
    free(pipeline->surface.context);
}
