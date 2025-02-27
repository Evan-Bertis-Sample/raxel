#include "red_pass.h"
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>
#include <raxel/core/util.h>

// This pass simply clears the image at swapchain.targets[0] to red.

static void red_pass_on_begin(raxel_pipeline_pass_t *pass, raxel_pipeline_globals *globals) {
    // Allocate a temporary command buffer from the graphics command pool.
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

    // Define the clear color (red).
    VkClearColorValue clear_color = { { 1.0f, 0.0f, 0.0f, 1.0f } };
    VkImageSubresourceRange range = {0};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    
    // For demonstration, we clear the first swapchain image.
    // In our abstraction, the swapchain is stored in globals->swapchain.
    // We assume __create_swapchain() has populated globals->swapchain.targets.
    if (globals->swapchain.targets == NULL || globals->swapchain.image_count < 1) {
        fprintf(stderr, "Swapchain targets not available for red pass!\n");
        exit(EXIT_FAILURE);
    }
    VkImage target_image = globals->swapchain.targets[0].image;
    
    // Record a clear command.
    // (Assume the image is already in a layout that permits clearing.)
    vkCmdClearColorImage(cmd_buf, target_image, VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &range);

    VK_CHECK(vkEndCommandBuffer(cmd_buf));

    // Store the recorded command buffer in the pass's resources.
    pass->resources.command_buffer = cmd_buf;
}

static void red_pass_on_end(raxel_pipeline_pass_t *pass, raxel_pipeline_globals *globals) {
    // Submit the command buffer recorded in on_begin.
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pass->resources.command_buffer;

    // (For simplicity, we use no semaphores here.)
    VK_CHECK(vkQueueSubmit(globals->queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(globals->queue_graphics);

    // Free the command buffer.
    vkFreeCommandBuffers(globals->device, globals->cmd_pool_graphics, 1, &pass->resources.command_buffer);
}

raxel_pipeline_pass_t red_pass_create(void) {
    raxel_pipeline_pass_t pass = {0};
    // Create a name for the pass.
    raxel_allocator_t allocator = raxel_default_allocator();
    pass.name = raxel_string_create(&allocator, strlen("red_pass") + 1);
    raxel_string_append(&pass.name, "red_pass");
    pass.on_begin = red_pass_on_begin;
    pass.on_end = red_pass_on_end;
    pass.pass_data = NULL;
    return pass;
}
