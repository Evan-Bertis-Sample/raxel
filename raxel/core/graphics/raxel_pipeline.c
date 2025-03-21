#include "raxel_pipeline.h"

#include <GLFW/glfw3.h>
#include <limits.h>
#include <raxel/core/graphics/vk.h>
#include <raxel/core/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raxel_surface.h"

// -----------------------------------------------------------------------------
// Internal helper functions (static, snake_case)
// -----------------------------------------------------------------------------

static void __create_instance(raxel_pipeline_globals_t *globals) {
    VkApplicationInfo app_info = {0};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "raxel_pipeline_renderer";
    app_info.apiVersion = VK_API_VERSION_1_2;

    // Query required extensions from GLFW.
    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    if (!glfw_extensions) {
        fprintf(stderr, "Failed to get required GLFW extensions\n");
        exit(EXIT_FAILURE);
    }

    VkInstanceCreateInfo create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = glfw_extension_count;
    create_info.ppEnabledExtensionNames = glfw_extensions;

    VK_CHECK(vkCreateInstance(&create_info, NULL, &globals->instance));
}

static void __pick_physical_device(raxel_pipeline_globals_t *globals) {
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

static void __create_logical_device(raxel_pipeline_globals_t *globals) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(globals->device_physical, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = malloc(sizeof(VkQueueFamilyProperties) * queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(globals->device_physical, &queue_family_count, queue_families);
    globals->index_graphics_queue_family = -1;
    globals->index_compute_queue_family = -1;
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if ((queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
            (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            globals->index_graphics_queue_family = i;
            globals->index_compute_queue_family = i;
            break;
        }
    }
    free(queue_families);

    if (globals->index_graphics_queue_family == -1 || globals->index_compute_queue_family == -1) {
        RAXEL_CORE_FATAL_ERROR("Failed to find suitable queue families\n");
        exit(EXIT_FAILURE);
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {0};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = globals->index_graphics_queue_family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priority;

    // Enable the swapchain extension.
    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo device_info = {0};
    device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    device_info.enabledExtensionCount = 1;
    device_info.ppEnabledExtensionNames = device_extensions;

    VK_CHECK(vkCreateDevice(globals->device_physical, &device_info, NULL, &globals->device));

    vkGetDeviceQueue(globals->device, globals->index_graphics_queue_family, 0, &globals->queue_graphics);
    vkGetDeviceQueue(globals->device, globals->index_compute_queue_family, 0, &globals->queue_compute);
}

static void __create_command_pools(raxel_pipeline_globals_t *globals) {
    VkCommandPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = globals->index_graphics_queue_family;
    VK_CHECK(vkCreateCommandPool(globals->device, &pool_info, NULL, &globals->cmd_pool_graphics));

    pool_info.queueFamilyIndex = globals->index_compute_queue_family;
    VK_CHECK(vkCreateCommandPool(globals->device, &pool_info, NULL, &globals->cmd_pool_compute));
}

// Create synchronization objects (semaphores) for presentation.
static void __create_sync_objects(raxel_pipeline_globals_t *globals) {
    VkSemaphoreCreateInfo sem_info = {0};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(globals->device, &sem_info, NULL, &globals->image_available_semaphore));
    VK_CHECK(vkCreateSemaphore(globals->device, &sem_info, NULL, &globals->render_finished_semaphore));
}

// -----------------------------------------------------------------------------
// Swapchain and target helpers
// -----------------------------------------------------------------------------

static int __create_swapchain(raxel_pipeline_globals_t *globals, int width, int height, raxel_pipeline_swapchain_t *swapchain) {
    VkSurfaceCapabilitiesKHR surface_caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(globals->device_physical, globals->surface->context.vk_surface, &surface_caps));

    // Determine desired image count.
    uint32_t desired_image_count = surface_caps.minImageCount + 1;
    if (surface_caps.maxImageCount > 0 && desired_image_count > surface_caps.maxImageCount) {
        desired_image_count = surface_caps.maxImageCount;
    }

    // Use the current extent if provided.
    VkExtent2D extent;
    if (surface_caps.currentExtent.width != UINT32_MAX) {
        extent = surface_caps.currentExtent;
    } else {
        extent.width = width;
        extent.height = height;
    }

    VkSwapchainCreateInfoKHR create_info = {0};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = globals->surface->context.vk_surface;
    create_info.minImageCount = desired_image_count;
    create_info.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = surface_caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create_info.clipped = VK_TRUE;
    VK_CHECK(vkCreateSwapchainKHR(globals->device, &create_info, NULL, &swapchain->swapchain));

    vkGetSwapchainImagesKHR(globals->device, swapchain->swapchain, &swapchain->image_count, NULL);
    VkImage *images = raxel_malloc(&globals->allocator, sizeof(VkImage) * swapchain->image_count);
    vkGetSwapchainImagesKHR(globals->device, swapchain->swapchain, &swapchain->image_count, images);

    swapchain->targets = malloc(sizeof(raxel_pipeline_target_t) * swapchain->image_count);
    for (uint32_t i = 0; i < swapchain->image_count; i++) {
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = images[i];
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_B8G8R8A8_UNORM;
        view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(globals->device, &view_info, NULL, &swapchain->targets[i].view));
        swapchain->targets[i].type = (raxel_pipeline_target_type_t)i;
        swapchain->targets[i].image = images[i];
        swapchain->targets[i].memory = VK_NULL_HANDLE;
    }
    raxel_free(&globals->allocator, images);
    swapchain->image_format = VK_FORMAT_B8G8R8A8_UNORM;
    swapchain->extent = extent;
    return 0;
}

static void __destroy_swapchain(raxel_pipeline_globals_t *globals, raxel_pipeline_swapchain_t *swapchain) {
    if (swapchain->targets) {  // Only proceed if targets have been allocated.
        for (uint32_t i = 0; i < swapchain->image_count; i++) {
            if (swapchain->targets[i].view != VK_NULL_HANDLE) {
                vkDestroyImageView(globals->device, swapchain->targets[i].view, NULL);
                swapchain->targets[i].view = VK_NULL_HANDLE;  // Mark as destroyed.
            }
        }
        free(swapchain->targets);
        swapchain->targets = NULL;  // Prevent double free.
    }
    if (swapchain->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(globals->device, swapchain->swapchain, NULL);
        swapchain->swapchain = VK_NULL_HANDLE;
    }
}

static int __create_targets(raxel_pipeline_globals_t *globals, raxel_pipeline_targets_t *targets, int width, int height) {
    // ----------------------------------------------------------
    // 1) Create a color image (R32G32B32A32_SFLOAT)
    // ----------------------------------------------------------
    {
        VkImageCreateInfo image_info = {0};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateImage(
            globals->device,
            &image_info,
            NULL,
            &targets->internal[RAXEL_PIPELINE_TARGET_COLOR].image));

        // Figure out memory requirements.
        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(
            globals->device,
            targets->internal[RAXEL_PIPELINE_TARGET_COLOR].image,
            &mem_reqs);

        // Query memory properties from our physical device.
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(globals->device_physical, &mem_props);

        // Find a memory type that is allowed and device-local.
        uint32_t memoryTypeIndex = UINT32_MAX;
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
            VkMemoryPropertyFlags flags = mem_props.memoryTypes[i].propertyFlags;
            if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memoryTypeIndex = i;
                break;
            }
        }
        if (memoryTypeIndex == UINT32_MAX) {
            fprintf(stderr, "Failed to find a device-local memory type for color image.\n");
            exit(EXIT_FAILURE);
        }

        // Allocate and bind memory to the image.
        VkMemoryAllocateInfo alloc_info = {0};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memoryTypeIndex;
        VK_CHECK(vkAllocateMemory(
            globals->device,
            &alloc_info,
            NULL,
            &targets->internal[RAXEL_PIPELINE_TARGET_COLOR].memory));

        VK_CHECK(vkBindImageMemory(
            globals->device,
            targets->internal[RAXEL_PIPELINE_TARGET_COLOR].image,
            targets->internal[RAXEL_PIPELINE_TARGET_COLOR].memory,
            0));

        // Create an ImageView for the color image.
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = targets->internal[RAXEL_PIPELINE_TARGET_COLOR].image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(
            globals->device,
            &view_info,
            NULL,
            &targets->internal[RAXEL_PIPELINE_TARGET_COLOR].view));

        targets->internal[RAXEL_PIPELINE_TARGET_COLOR].type =
            RAXEL_PIPELINE_TARGET_COLOR;
    }

    // ----------------------------------------------------------
    // 2) Create a depth image (D32_SFLOAT)
    // ----------------------------------------------------------
    {
        VkImageCreateInfo image_info = {0};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_D32_SFLOAT;  // typical depth-only format
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateImage(
            globals->device,
            &image_info,
            NULL,
            &targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].image));

        // Figure out memory requirements for the depth image.
        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(
            globals->device,
            targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].image,
            &mem_reqs);

        // Same memory property search logic as above.
        VkPhysicalDeviceMemoryProperties mem_props;
        vkGetPhysicalDeviceMemoryProperties(globals->device_physical, &mem_props);

        uint32_t memoryTypeIndex = UINT32_MAX;
        for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
            VkMemoryPropertyFlags flags = mem_props.memoryTypes[i].propertyFlags;
            if ((mem_reqs.memoryTypeBits & (1 << i)) &&
                (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
                memoryTypeIndex = i;
                break;
            }
        }
        if (memoryTypeIndex == UINT32_MAX) {
            fprintf(stderr, "Failed to find a device-local memory type for depth image.\n");
            exit(EXIT_FAILURE);
        }

        // Allocate and bind.
        VkMemoryAllocateInfo alloc_info = {0};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = memoryTypeIndex;
        VK_CHECK(vkAllocateMemory(
            globals->device,
            &alloc_info,
            NULL,
            &targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].memory));

        VK_CHECK(vkBindImageMemory(
            globals->device,
            targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].image,
            targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].memory,
            0));

        // Create an ImageView for the depth image.
        VkImageViewCreateInfo view_info = {0};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_D32_SFLOAT;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;  // depth aspect
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(
            globals->device,
            &view_info,
            NULL,
            &targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].view));

        targets->internal[RAXEL_PIPELINE_TARGET_DEPTH].type =
            RAXEL_PIPELINE_TARGET_DEPTH;
    }

    // Whichever target you want to “debug-blit” to the swapchain
    // or otherwise sample from can be set here:
    targets->debug_target = RAXEL_PIPELINE_TARGET_COLOR;

    return 0;
}

static void __create_descriptor_pool(raxel_pipeline_t *pipeline) {
    VkDescriptorPoolSize pool_sizes[3] = {0};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = 1;
    pool_sizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER; // Added for large data buffers.
    pool_sizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info = {0};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 3;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 1;
    VK_CHECK(vkCreateDescriptorPool(pipeline->resources.device, &pool_info, NULL, &pipeline->resources.descriptor_pool));
}

// Record and submit a command buffer to blit the selected target to a swapchain image.
static void __present_target(raxel_pipeline_globals_t *globals, raxel_pipeline_targets_t *targets, raxel_pipeline_swapchain_t *swapchain) {
    VkImage src_image = targets->internal[targets->debug_target].image;

    // Create a temporary command buffer from the graphics command pool.
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = globals->cmd_pool_graphics;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf;
    VK_CHECK(vkAllocateCommandBuffers(globals->device, &alloc_info, &cmd_buf));

    // Begin recording.
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buf, &begin_info));

    // (Transition src_image and the swapchain image to proper layouts, then blit.)
    // For simplicity, assume the images are already in TRANSFER_SRC and TRANSFER_DST layouts.
    // Record a blit command:
    VkImageBlit blit = {0};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcSubresource.mipLevel = 0;
    blit.srcOffsets[0] = (VkOffset3D){0, 0, 0};
    blit.srcOffsets[1] = (VkOffset3D){(int32_t)swapchain->extent.width, (int32_t)swapchain->extent.height, 1};
    blit.dstSubresource = blit.srcSubresource;
    blit.dstOffsets[0] = (VkOffset3D){0, 0, 0};
    blit.dstOffsets[1] = (VkOffset3D){(int32_t)swapchain->extent.width, (int32_t)swapchain->extent.height, 1};

    // For brevity, we assume one swapchain image.
    // In production, you should acquire an image index via vkAcquireNextImageKHR.
    uint32_t image_index = 0;

    // Transition swapchain image to TRANSFER_DST layout (omitted: add barriers).
    vkCmdBlitImage(cmd_buf, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapchain->targets[image_index].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);
    // Transition swapchain image back to PRESENT_SRC layout (omitted: add barriers).

    VK_CHECK(vkEndCommandBuffer(cmd_buf));

    // Submit the command buffer.
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &globals->image_available_semaphore;
    VkPipelineStageFlags wait_stages = VK_PIPELINE_STAGE_TRANSFER_BIT;
    submit_info.pWaitDstStageMask = &wait_stages;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &globals->render_finished_semaphore;
    VK_CHECK(vkQueueSubmit(globals->queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(globals->queue_graphics);
    vkFreeCommandBuffers(globals->device, globals->cmd_pool_graphics, 1, &cmd_buf);

    // Present the image.
    VkPresentInfoKHR present_info = {0};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &globals->render_finished_semaphore;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain->swapchain;
    present_info.pImageIndices = &image_index;
    VK_CHECK(vkQueuePresentKHR(globals->queue_graphics, &present_info));
}

static void __destroy_targets(raxel_pipeline_t *pipeline) {
    for (raxel_size_t i = 0; i < RAXEL_PIPELINE_TARGET_COUNT; i++) {
        raxel_pipeline_target_t *target = &pipeline->resources.targets.internal[i];
        if (target->view != VK_NULL_HANDLE) {
            vkDestroyImageView(pipeline->resources.device, target->view, NULL);
            target->view = VK_NULL_HANDLE;  // Prevent double-destroy.
        }
        if (target->image != VK_NULL_HANDLE) {
            vkDestroyImage(pipeline->resources.device, target->image, NULL);
            target->image = VK_NULL_HANDLE;
        }
        if (target->memory != VK_NULL_HANDLE) {
            vkFreeMemory(pipeline->resources.device, target->memory, NULL);
            target->memory = VK_NULL_HANDLE;
        }
    }
}

// -----------------------------------------------------------------------------
// Public API implementations.
// -----------------------------------------------------------------------------

raxel_pipeline_t *raxel_pipeline_create(raxel_allocator_t *allocator, raxel_surface_t *surface) {
    raxel_pipeline_t *pipeline = raxel_malloc(allocator, sizeof(raxel_pipeline_t));
    pipeline->resources.allocator = *allocator;
    pipeline->resources.instance = VK_NULL_HANDLE;
    pipeline->resources.device_physical = VK_NULL_HANDLE;
    pipeline->resources.device = VK_NULL_HANDLE;
    pipeline->resources.queue_graphics = VK_NULL_HANDLE;
    pipeline->resources.queue_compute = VK_NULL_HANDLE;
    pipeline->resources.index_graphics_queue_family = 0;
    pipeline->resources.index_compute_queue_family = 0;
    pipeline->resources.cmd_pool_graphics = VK_NULL_HANDLE;
    pipeline->resources.cmd_pool_compute = VK_NULL_HANDLE;
    pipeline->resources.surface = surface;
    pipeline->passes = raxel_list_create_reserve(raxel_pipeline_pass_t, &pipeline->resources.allocator, 4);
    return pipeline;
}

void raxel_pipeline_destroy(raxel_pipeline_t *pipeline) {
    raxel_list_destroy(pipeline->passes);
    raxel_free(&pipeline->resources.allocator, pipeline);
}

void raxel_pipeline_add_pass(raxel_pipeline_t *pipeline, raxel_pipeline_pass_t pass) {
    raxel_list_push_back(pipeline->passes, pass);
}

raxel_size_t raxel_pipeline_num_passes(raxel_pipeline_t *pipeline) {
    return raxel_list_size(pipeline->passes);
}

raxel_pipeline_pass_t *raxel_pipeline_get_pass_by_index(raxel_pipeline_t *pipeline, raxel_size_t index) {
    return &pipeline->passes[index];
}

raxel_pipeline_pass_t *raxel_pipeline_get_pass_by_name(raxel_pipeline_t *pipeline, raxel_string_t name) {
    raxel_size_t num = raxel_list_size(pipeline->passes);
    for (size_t i = 0; i < num; i++) {
        raxel_pipeline_pass_t *pass = &pipeline->passes[i];
        if (raxel_string_compare(&pass->name, &name) == 0) {
            return pass;
        }
    }
    return NULL;
}

int raxel_pipeline_initialize(raxel_pipeline_t *pipeline) {
    RAXEL_CORE_LOG("Creating Vulkan instance\n");
    __create_instance(&pipeline->resources);
    RAXEL_CORE_LOG("Picking physical device\n");
    __pick_physical_device(&pipeline->resources);
    RAXEL_CORE_LOG("Creating logical device\n");
    __create_logical_device(&pipeline->resources);
    RAXEL_CORE_LOG("Creating command pools\n");
    __create_command_pools(&pipeline->resources);
    RAXEL_CORE_LOG("Creating synchronization objects\n");
    __create_sync_objects(&pipeline->resources);
    // Create swapchain using surface dimensions.
    RAXEL_CORE_LOG("Initializing surface\n");
    raxel_surface_initialize(pipeline->resources.surface, pipeline->resources.instance);
    RAXEL_CORE_LOG("Creating swapchain\n");
    __create_swapchain(&pipeline->resources, pipeline->resources.surface->width, pipeline->resources.surface->height, &pipeline->resources.swapchain);
    RAXEL_CORE_LOG("Creating targets\n");
    __create_targets(&pipeline->resources, &pipeline->resources.targets, pipeline->resources.surface->width, pipeline->resources.surface->height);
    RAXEL_CORE_LOG("Creating descriptor pool\n");
    __create_descriptor_pool(pipeline);
    RAXEL_CORE_LOG("Pipeline initialized\n");
    return 0;
}

void raxel_pipeline_set_debug_target(raxel_pipeline_t *pipeline, raxel_pipeline_target_type_t target) {
    pipeline->resources.targets.debug_target = target;
}

void raxel_pipeline_present(raxel_pipeline_t *pipeline) {
    __present_target(&pipeline->resources, &pipeline->resources.targets, &pipeline->resources.swapchain);
}

void raxel_pipeline_run(raxel_pipeline_t *pipeline) {
    raxel_pipeline_start(pipeline);
    while (!raxel_pipeline_should_close(pipeline)) {
        raxel_pipeline_update(pipeline);
    }
    vkDeviceWaitIdle(pipeline->resources.device);
}

void raxel_pipeline_start(raxel_pipeline_t *pipeline) {
    raxel_size_t num_passes = raxel_list_size(pipeline->passes);
    for (size_t i = 0; i < num_passes; i++) {
        raxel_pipeline_pass_t *pass = &pipeline->passes[i];
        if (pass->initialize) {
            pass->initialize(pass, &pipeline->resources);
        }
    }
}

int raxel_pipeline_should_close(raxel_pipeline_t *pipeline) {
    return glfwWindowShouldClose(pipeline->resources.surface->context.window);
}

void raxel_pipeline_update(raxel_pipeline_t *pipeline) {
    if (raxel_surface_update(pipeline->resources.surface) != 0) {
        return;
    }

    raxel_size_t num_passes = raxel_list_size(pipeline->passes);
    for (size_t i = 0; i < num_passes; i++) {
        raxel_pipeline_pass_t *pass = &pipeline->passes[i];
        if (pass->on_begin) {
            // RAXEL_CORE_LOG("Running pass %s\n", raxel_string_to_cstr(&pass->name));
            pass->on_begin(pass, &pipeline->resources);
        }
        if (pass->on_end) {
            // RAXEL_CORE_LOG("Ending pass %s\n", raxel_string_to_cstr(&pass->name));
            pass->on_end(pass, &pipeline->resources);
        }
    }
    raxel_pipeline_present(pipeline);
}

void raxel_pipeline_cleanup(raxel_pipeline_t *pipeline) {
    // Destroy compute command pool if it is not the same as graphics command pool.
    if (pipeline->resources.cmd_pool_compute != VK_NULL_HANDLE) {
        if (pipeline->resources.cmd_pool_compute != pipeline->resources.cmd_pool_graphics) {
            vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.cmd_pool_compute, NULL);
        }
        pipeline->resources.cmd_pool_compute = VK_NULL_HANDLE;
    }

    // Always destroy the graphics command pool.
    if (pipeline->resources.cmd_pool_graphics != VK_NULL_HANDLE) {
        vkDestroyCommandPool(pipeline->resources.device, pipeline->resources.cmd_pool_graphics, NULL);
        pipeline->resources.cmd_pool_graphics = VK_NULL_HANDLE;
    }

    if (pipeline->resources.swapchain.swapchain != VK_NULL_HANDLE) {
        __destroy_swapchain(&pipeline->resources, &pipeline->resources.swapchain);
    }

    __destroy_targets(pipeline);

    vkDestroySurfaceKHR(pipeline->resources.instance, pipeline->resources.surface->context.vk_surface, NULL);

    raxel_surface_destroy(pipeline->resources.surface);

    if (pipeline->resources.device != VK_NULL_HANDLE) {
        vkDestroyDevice(pipeline->resources.device, NULL);
    }
    if (pipeline->resources.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(pipeline->resources.instance, NULL);
    }
}
