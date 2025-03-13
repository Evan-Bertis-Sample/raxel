#include "compute_pass.h"

#include <raxel/core/graphics.h>
#include <raxel/core/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

raxel_compute_shader_t *raxel_compute_shader_create(raxel_pipeline_t *pipeline, const char *shader_path) {
    VkDevice device = pipeline->resources.device;
    raxel_compute_shader_t *shader = malloc(sizeof(raxel_compute_shader_t));
    if (!shader) {
        fprintf(stderr, "Failed to allocate compute shader\n");
        exit(EXIT_FAILURE);
    }
    VkShaderModule comp_module = __load_shader_module(device, shader_path);

    // Create descriptor set layout bindings using the enum to avoid magic numbers.
    VkDescriptorSetLayoutBinding bindings[RAXEL_COMPUTE_BINDING_COUNT] = {0};

    // Binding for storage images.
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_IMAGE].binding = RAXEL_COMPUTE_BINDING_STORAGE_IMAGE;
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_IMAGE].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_IMAGE].descriptorCount = RAXEL_PIPELINE_TARGET_COUNT;
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_IMAGE].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Binding for storage buffer.
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_BUFFER].binding = RAXEL_COMPUTE_BINDING_STORAGE_BUFFER;
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_BUFFER].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_BUFFER].descriptorCount = 1;
    bindings[RAXEL_COMPUTE_BINDING_STORAGE_BUFFER].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {0};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = RAXEL_COMPUTE_BINDING_COUNT;
    layout_info.pBindings = bindings;
    VkDescriptorSetLayout desc_set_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layout_info, NULL, &desc_set_layout));

    // Create a pipeline layout with the descriptor set layout.
    VkPipelineLayoutCreateInfo pipeline_layout_info = {0};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &desc_set_layout;
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

    VkDescriptorSetAllocateInfo ds_alloc = {0};
    ds_alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ds_alloc.descriptorPool = pipeline->resources.descriptor_pool;
    ds_alloc.descriptorSetCount = 1;
    ds_alloc.pSetLayouts = &desc_set_layout;
    VK_CHECK(vkAllocateDescriptorSets(device, &ds_alloc, &shader->descriptor_set));

    // Clean up the temporary descriptor set layout.
    vkDestroyDescriptorSetLayout(device, desc_set_layout, NULL);

    shader->allocator = &pipeline->resources.allocator;

    return shader;
}


void raxel_compute_shader_destroy(raxel_compute_shader_t *shader, raxel_pipeline_t *pipeline) {
    VkDevice device = pipeline->resources.device;
    vkDestroyPipeline(device, shader->pipeline, NULL);
    vkDestroyPipelineLayout(device, shader->pipeline_layout, NULL);
    free(shader);
}

void raxel_compute_shader_set_pc(raxel_compute_shader_t *shader, raxel_pc_buffer_desc_t *desc) {
    shader->pc_buffer = raxel_pc_buffer_create(shader->allocator, desc);
}

void raxel_compute_shader_push_pc(raxel_compute_shader_t *shader, VkCommandBuffer cmd_buf) {
    if (shader->pc_buffer) {
        vkCmdPushConstants(cmd_buf,
                           shader->pipeline_layout,
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           shader->pc_buffer->data_size,
                           shader->pc_buffer->data);
    }
}

void raxel_compute_shader_set_sb(raxel_compute_shader_t *shader, raxel_pipeline_t *pipeline, raxel_sb_buffer_desc_t *desc) {
    // Create the storage buffer using the pipeline's allocator.
    shader->sb_buffer = raxel_sb_buffer_create(&pipeline->resources.allocator,
                                               desc,
                                               pipeline->resources.device,
                                               pipeline->resources.device_physical);

    // Update the compute shader's descriptor set.
    VkDescriptorBufferInfo bufferInfo = {0};
    bufferInfo.buffer = shader->sb_buffer->buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = shader->sb_buffer->data_size;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = shader->descriptor_set;
    write.dstBinding = RAXEL_COMPUTE_BINDING_STORAGE_BUFFER;  // use enum here
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(pipeline->resources.device, 1, &write, 0, NULL);
}

// -----------------------------------------------------------------------------
// Compute Pass Implementation
// -----------------------------------------------------------------------------

static void compute_pass_initialize(raxel_pipeline_pass_t *pass, raxel_pipeline_globals_t *globals) {
    raxel_compute_pass_context_t *ctx = (raxel_compute_pass_context_t *)pass->pass_data;
    VkDevice device = globals->device;

    // Count valid targets in the context's targets array.
    int valid_count = 0;
    for (int i = 0; i < RAXEL_PIPELINE_TARGET_COUNT; i++) {
        RAXEL_CORE_LOG("ctx->targets[%d]: %d\n", i, ctx->targets[i]);
        if (ctx->targets[i] < 0)
            break;
        valid_count++;
    }
    valid_count -= 1;  // Exclude the sentinel value.

    // Build an array of descriptor image infos from the pipeline's targets.
    ctx->image_infos = raxel_malloc(&globals->allocator, valid_count * sizeof(VkDescriptorImageInfo));
    RAXEL_CORE_LOG("valid_count: %d\n", valid_count);
    for (int i = 0; i < valid_count; i++) {
        int target_index = ctx->targets[i];
        ctx->image_infos[i].imageView = globals->targets.internal[target_index].view;
        ctx->image_infos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        ctx->image_infos[i].sampler = VK_NULL_HANDLE;
    }
    ctx->num_image_infos = valid_count;
}

static void compute_pass_on_begin(raxel_pipeline_pass_t *pass, raxel_pipeline_globals_t *globals) {
    raxel_compute_pass_context_t *ctx = (raxel_compute_pass_context_t *)pass->pass_data;
    VkDevice device = globals->device;

    VkWriteDescriptorSet write = {0};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = ctx->compute_shader->descriptor_set;
    write.dstBinding = RAXEL_COMPUTE_BINDING_STORAGE_IMAGE; // use enum here
    write.dstArrayElement = 0;
    write.descriptorCount = ctx->num_image_infos;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = ctx->image_infos;
    vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

    // Allocate a command buffer.
    VkCommandBufferAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = globals->cmd_pool_compute;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;
    VkCommandBuffer cmd_buf;
    VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &cmd_buf));
    pass->resources.command_buffer = cmd_buf;

    VkCommandBufferBeginInfo begin_info = {0};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd_buf, &begin_info));

    raxel_compute_shader_push_pc(ctx->compute_shader, cmd_buf);

    // Bind the compute pipeline.
    vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->compute_shader->pipeline);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, ctx->compute_shader->pipeline_layout,
                            0, 1, &ctx->compute_shader->descriptor_set, 0, NULL);

    // Dispatch compute work.
    vkCmdDispatch(cmd_buf, ctx->dispatch_x, ctx->dispatch_y, ctx->dispatch_z);

    VK_CHECK(vkEndCommandBuffer(cmd_buf));
}


static void compute_pass_on_end(raxel_pipeline_pass_t *pass, raxel_pipeline_globals_t *globals) {
    VkDevice device = globals->device;
    VkSubmitInfo submit_info = {0};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pass->resources.command_buffer;

    VK_CHECK(vkQueueSubmit(globals->queue_compute, 1, &submit_info, VK_NULL_HANDLE));
    vkQueueWaitIdle(globals->queue_compute);

    vkFreeCommandBuffers(device, globals->cmd_pool_compute, 1, &pass->resources.command_buffer);

    raxel_compute_pass_context_t *ctx = (raxel_compute_pass_context_t *)pass->pass_data;
    if (ctx->on_dispatch_finished) {
        ctx->on_dispatch_finished(ctx, globals);
    }
}

raxel_pipeline_pass_t raxel_compute_pass_create(raxel_compute_pass_context_t *context) {
    raxel_pipeline_pass_t pass = {0};
    raxel_allocator_t allocator = raxel_default_allocator();
    pass.name = raxel_string_create(&allocator, strlen("compute_pass") + 1);
    raxel_string_append(&pass.name, "compute_pass");
    pass.pass_data = context;
    pass.initialize = compute_pass_initialize;
    pass.on_begin = compute_pass_on_begin;
    pass.on_end = compute_pass_on_end;
    pass.allocator = allocator;

    raxel_compute_shader_set_pc(context->compute_shader, &context->pc_desc);

    return pass;
}
