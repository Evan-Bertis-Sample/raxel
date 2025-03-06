#include "red_pass.h"

#include <raxel/core/graphics/vk.h>
#include <raxel/core/util.h>
#include <stdlib.h>
#include <string.h>

// This pass clears the internal color target (stored at targets.internal[COLOR]) to red.

static void red_pass_initialize(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    // Allocate a temporary command buffer from the graphics command pool.
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pipeline->resources.cmd_pool_graphics;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf;
    VK_CHECK(vkAllocateCommandBuffers(pipeline->resources.device, &alloc_info, &cmd_buf));
    // Store the temporary command buffer.
    pass->resources.command_buffer = cmd_buf;
}

static void red_pass_on_begin(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    // Begin recording.
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(pass->resources.command_buffer, &begin_info));

    // Define the clear color (red).
    VkClearColorValue clear_color = {{1.0f, 0.0f, 0.0f, 1.0f}};
    VkImageSubresourceRange range = {0};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;

    // Clear the internal color target.
    VkImage target_image = pipeline->targets.internal[RAXEL_PIPELINE_TARGET_COLOR].image;

    // Before clearing, we must ensure the image is in a layout that permits clear.
    // For this simplified example, assume it is already in VK_IMAGE_LAYOUT_GENERAL.
    vkCmdClearColorImage(pass->resources.command_buffer, target_image, VK_IMAGE_LAYOUT_GENERAL, &clear_color, 1, &range);

    VK_CHECK(vkEndCommandBuffer(pass->resources.command_buffer));
}

static void red_pass_on_end(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    // Submit the command buffer recorded in on_begin.
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pass->resources.command_buffer;
    // For simplicity, no semaphores here.
    VK_CHECK(vkQueueSubmit(pipeline->resources.queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(pipeline->resources.queue_graphics);

    // Free the temporary command buffer.
    vkFreeCommandBuffers(pipeline->resources.device, pipeline->resources.cmd_pool_graphics, 1, &pass->resources.command_buffer);
}

raxel_pipeline_pass_t red_pass_create(void) {
    raxel_pipeline_pass_t pass = {0};
    raxel_allocator_t allocator = raxel_default_allocator();
    pass.name = raxel_string_create(&allocator, strlen("red_pass") + 1);
    raxel_string_append(&pass.name, "red_pass");
    pass.initialize = red_pass_initialize;
    pass.on_begin = red_pass_on_begin;
    pass.on_end = red_pass_on_end;
    pass.pass_data = NULL;
    return pass;
}