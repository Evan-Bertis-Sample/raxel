#include "compute_pass.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include <raxel/core/util.h>
#include <raxel/core/graphics.h>

// -----------------------------------------------------------------------------
// Compute Shader Implementation
// -----------------------------------------------------------------------------

// Internal helper: load a shader module from a SPIR-V file.
static VkShaderModule __load_shader_module(VkDevice device, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open shader file: %s\n", path);
        exit(EXIT_FAILURE);
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *code = malloc(size);
    if (!code) {
        fprintf(stderr, "Failed to allocate memory for shader code\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }
    fread(code, 1, size, file);
    fclose(file);
    VkShaderModuleCreateInfo module_info = {0};
    module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_info.codeSize = size;
    module_info.pCode = (uint32_t *)code;
    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &module_info, NULL, &shader_module));
    free(code);
    return shader_module;
}

raxel_compute_shader_t *raxel_compute_shader_create(raxel_pipeline_t *pipeline, const char *shader_path, size_t push_data_size) {
    VkDevice device = pipeline->resources.device;
    raxel_compute_shader_t *shader = malloc(sizeof(raxel_compute_shader_t));
    if (!shader) {
        fprintf(stderr, "Failed to allocate compute shader\n");
        exit(EXIT_FAILURE);
    }
    VkShaderModule comp_module = __load_shader_module(device, shader_path);
    
    VkPushConstantRange pc_range = {0};
    pc_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pc_range.offset = 0;
    pc_range.size = push_data_size;
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &pc_range;
    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, NULL, &shader->pipeline_layout));
    
    VkPipelineShaderStageCreateInfo stage_info = {0};
    stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_info.module = comp_module;
    stage_info.pName = "main";
    
    VkComputePipelineCreateInfo pipeline_info = {0};
    pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeline_info.stage = stage_info;
    pipeline_info.layout = shader->pipeline_layout;
    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &shader->pipeline));
    
    vkDestroyShaderModule(device, comp_module, NULL);
    
    shader->__push_layout = raxel_list_create_size(raxel_push_constant_entry_t, &pipeline->resources.allocator, 0);
    // For demonstration, add two entries: "view" (offset 0, size 64) and "fov" (offset 64, size 4).
    raxel_push_constant_entry_t entry_view = { "view", 0, 64 };
    raxel_list_push_back(shader->__push_layout, entry_view);
    raxel_push_constant_entry_t entry_fov = { "fov", 64, 4 };
    raxel_list_push_back(shader->__push_layout, entry_fov);
    
    shader->push_data_size = push_data_size;
    shader->push_data = malloc(push_data_size);
    memset(shader->push_data, 0, push_data_size);
    shader->descriptor_set = VK_NULL_HANDLE;
    return shader;
}

void raxel_compute_shader_destroy(raxel_compute_shader_t *shader, raxel_pipeline_t *pipeline) {
    VkDevice device = pipeline->resources.device;
    vkDestroyPipeline(device, shader->pipeline, NULL);
    vkDestroyPipelineLayout(device, shader->pipeline_layout, NULL);
    free(shader->push_data);
    raxel_list_destroy(shader->__push_layout);
    free(shader);
}

void raxel_compute_shader_push_constant(raxel_compute_shader_t *shader, const char *name, const void *data) {
    size_t count = raxel_list_size(shader->__push_layout);
    for (size_t i = 0; i < count; i++) {
        raxel_push_constant_entry_t *entry = &shader->__push_layout[i];
        if (strcmp(entry->name, name) == 0) {
            memcpy((char *)shader->push_data + entry->offset, data, entry->size);
            return;
        }
    }
    fprintf(stderr, "Push constant '%s' not found!\n", name);
}

// -----------------------------------------------------------------------------
// Compute Pass Implementation
// -----------------------------------------------------------------------------

static void compute_pass_on_begin(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    raxel_compute_pass_context_t *ctx = (raxel_compute_pass_context_t *)pass->pass_data;
    VkDevice device = pipeline->resources.device;
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pipeline->resources.cmd_pool_compute;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf;
    VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd_buf));
    pass->resources.command_buffer = cmd_buf;
    
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buf, &begin_info));
    
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->compute_shader->pipeline);
    if (ctx->compute_shader->descriptor_set != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->compute_shader->pipeline_layout,
                                0, 1, &ctx->compute_shader->descriptor_set, 0, NULL);
    }
    
    vkCmdPushConstants(cmd_buf, ctx->compute_shader->pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                       0, ctx->compute_shader->push_data_size, ctx->compute_shader->push_data);
    
    vkCmdDispatch(cmd_buf, ctx->dispatch_x, ctx->dispatch_y, ctx->dispatch_z);
    
    VK_CHECK(vkEndCommandBuffer(cmd_buf));
}

static void compute_pass_on_end(raxel_pipeline_pass_t *pass, raxel_pipeline_t *pipeline) {
    VkDevice device = pipeline->resources.device;
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pass->resources.command_buffer;
    
    VK_CHECK(vkQueueSubmit(pipeline->resources.queue_compute, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(pipeline->resources.queue_compute);
    
    vkFreeCommandBuffers(device, pipeline->resources.cmd_pool_compute, 1, &pass->resources.command_buffer);
    
    raxel_compute_pass_context_t *ctx = (raxel_compute_pass_context_t *)pass->pass_data;
    if (ctx->on_dispatch_finished) {
        ctx->on_dispatch_finished(ctx, pipeline);
    } else {
        raxel_compute_pass_blit(ctx, pipeline);
    }
}

// Default blit callback: Copy the computed output into the pipeline target.
// This function records a command buffer to blit from the compute shader's output
// (if set in the context; otherwise, from the internal target indicated by blit_target)
// to the pipeline target image, then calls raxel_pipeline_present().
void raxel_compute_pass_blit(raxel_compute_pass_context_t *context, raxel_pipeline_t *pipeline) {
    VkDevice device = pipeline->resources.device;
    // Determine the source image.
    VkImage src_image = (context->output_image != VK_NULL_HANDLE) ? context->output_image
                        : pipeline->targets.internal[context->blit_target].image;
    
    // Destination: the pipeline target (we assume it matches the swapchain extent).
    VkImage dst_image = pipeline->targets.internal[context->blit_target].image;
    
    // Allocate a temporary command buffer from the graphics command pool.
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = pipeline->resources.cmd_pool_graphics;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf;
    VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd_buf));
    
    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buf, &begin_info));
    
    // Set up a blit from src_image to dst_image.
    VkExtent2D extent = pipeline->resources.swapchain.extent;
    VkImageBlit blit = {0};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = 0;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.srcOffsets[0] = (VkOffset3D){0, 0, 0};
    blit.srcOffsets[1] = (VkOffset3D){(int32_t)extent.width, (int32_t)extent.height, 1};
    blit.dstSubresource = blit.srcSubresource;
    blit.dstOffsets[0] = (VkOffset3D){0, 0, 0};
    blit.dstOffsets[1] = (VkOffset3D){(int32_t)extent.width, (int32_t)extent.height, 1};
    
    // For simplicity, assume images are already in the correct layouts.
    vkCmdBlitImage(cmd_buf, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);
    
    VK_CHECK(vkEndCommandBuffer(cmd_buf));
    
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf;
    VK_CHECK(vkQueueSubmit(pipeline->resources.queue_graphics, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(pipeline->resources.queue_graphics);
    vkFreeCommandBuffers(device, pipeline->resources.cmd_pool_graphics, 1, &cmd_buf);
}

raxel_pipeline_pass_t raxel_compute_pass_create(raxel_compute_pass_context_t *context) {
    raxel_pipeline_pass_t pass = {0};
    raxel_allocator_t allocator = raxel_default_allocator();
    pass.name = raxel_string_create(&allocator, strlen("compute_pass") + 1);
    raxel_string_append(&pass.name, "compute_pass");
    pass.pass_data = context;
    pass.on_begin = compute_pass_on_begin;
    pass.on_end = compute_pass_on_end;
    return pass;
}
