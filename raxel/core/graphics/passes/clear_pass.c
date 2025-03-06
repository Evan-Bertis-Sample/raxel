#include "clear_color_pass.h"


#include <raxel/core/graphics/vk.h>
#include <raxel/core/util.h>
#include <stdlib.h>
#include <string.h>

// This structure holds pass-specific data for the clear-color pass.
typedef struct __clear_color_pass_data {
    VkClearColorValue clear_color;
} __clear_color_pass_data_t;

// Helper to allocate a temporary command buffer; can be called in on_begin.
static void __clear_color_pass_initialize(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pipeline->resources.cmd_pool_graphics;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf;
    VK_CHECK(vkAllocateCommandBuffers(pipeline->resources.device, &alloc_info, &cmd_buf));
    pass->resources.command_buffer = cmd_buf;
}

// on_begin callback: record commands to clear the internal color target.
static void clear_color_pass_on_begin(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    __clear_color_pass_initialize(pass, pipeline);
    
    VkCommandBuffer cmd_buf = pass->resources.command_buffer;
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buf, &begin_info));
    
    // Retrieve the clear color from pass data.
    __clear_color_pass_data_t *data = (__clear_color_pass_data_t *)pass->pass_data;
    
    VkImageSubresourceRange range = {0};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    
    // Clear the internal color target.
    VkImage target_image = pipeline->targets.internal[RAXEL_PIPELINE_TARGET_COLOR].image;
    // Assuming the image is in VK_IMAGE_LAYOUT_GENERAL.
    vkCmdClearColorImage(cmd_buf, target_image, VK_IMAGE_LAYOUT_GENERAL, &data->clear_color, 1, &range);
    
    VK_CHECK(vkEndCommandBuffer(cmd_buf));
}

// on_end callback: submit the command buffer and free it.
static void clear_color_pass_on_end(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pass->resources.command_buffer;
    
    VK_CHECK(vkQueueSubmit(pipeline->resources.queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(pipeline->resources.queue_graphics);
    
    vkFreeCommandBuffers(pipeline->resources.device, pipeline->resources.cmd_pool_graphics, 1, &pass->resources.command_buffer);
}

// Public API: create a clear-color pass using a cglm vec4.
raxel_pipeline_pass_t clear_color_pass_create(vec4 clear_color) {
    raxel_pipeline_pass_t pass = {0};
    raxel_allocator_t allocator = raxel_default_allocator();
    pass.name = raxel_string_create(&allocator, strlen("clear_color_pass") + 1);
    raxel_string_append(&pass.name, "clear_color_pass");
    
    // Allocate pass-specific data.
    __clear_color_pass_data_t *data = raxel_malloc(&allocator, sizeof(__clear_color_pass_data_t));
    if (!data) {
        fprintf(stderr, "Failed to allocate clear_color_pass_data\n");
        exit(EXIT_FAILURE);
    }
    // Convert the cglm vec4 to a VkClearColorValue.
    data->clear_color.float32[0] = clear_color[0];
    data->clear_color.float32[1] = clear_color[1];
    data->clear_color.float32[2] = clear_color[2];
    data->clear_color.float32[3] = clear_color[3];


    pass.pass_data = data;
    pass.on_begin = clear_color_pass_on_begin;
    pass.on_end = clear_color_pass_on_end;
    pass.allocator = allocator;
    
    return pass;
}
