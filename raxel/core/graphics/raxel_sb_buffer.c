#include "raxel_sb_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <raxel/core/graphics.h>

raxel_sb_buffer_t *raxel_sb_buffer_create(raxel_allocator_t *allocator,
                                          raxel_sb_buffer_desc_t *desc,
                                          VkDevice device,
                                          VkPhysicalDevice physicalDevice)
{
    raxel_sb_buffer_t *buffer = raxel_malloc(allocator, sizeof(raxel_sb_buffer_t));
    buffer->allocator = allocator;
    buffer->entries = raxel_array_create(raxel_sb_entry_t, allocator, desc->entry_count);
    buffer->data_size = 0;

    // Copy entries and calculate total size.
    for (raxel_size_t i = 0; i < desc->entry_count; i++) {
        // You might wish to duplicate the string if needed.
        buffer->entries[i] = desc->entries[i];
        uint32_t field_end = desc->entries[i].offset + desc->entries[i].size;
        if (field_end > buffer->data_size) {
            buffer->data_size = field_end;
        }
    }

    RAXEL_CORE_LOG("Allocating storage buffer of size %u\n", buffer->data_size);
    
    // Allocate CPU–side memory.
    buffer->data = raxel_malloc(allocator, buffer->data_size);

    // Create the Vulkan buffer
    VkBufferCreateInfo buf_info = {0};
    buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buf_info.size = buffer->data_size;
    buf_info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device, &buf_info, NULL, &buffer->buffer));

    // Query memory requirements.
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(device, buffer->buffer, &mem_req);

    // Find a memory type that is host-visible and coherent.
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &mem_props);
    uint32_t memory_type_index = UINT32_MAX;
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((mem_req.memoryTypeBits & (1 << i)) &&
            (mem_props.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            memory_type_index = i;
            break;
        }
    }
    if (memory_type_index == UINT32_MAX) {
        RAXEL_CORE_FATAL_ERROR("Failed to find a host-visible memory type for storage buffer\n");
    }

    VkMemoryAllocateInfo alloc_info = {0};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_req.size;
    alloc_info.memoryTypeIndex = memory_type_index;
    VK_CHECK(vkAllocateMemory(device, &alloc_info, NULL, &buffer->memory));
    VK_CHECK(vkBindBufferMemory(device, buffer->buffer, buffer->memory, 0));

    return buffer;
}

void *raxel_sb_buffer_get(raxel_sb_buffer_t *buffer, char *name)
{
    for (raxel_size_t i = 0; i < raxel_array_size(buffer->entries); i++) {
        raxel_sb_entry_t *entry = &buffer->entries[i];
        if (strcmp(entry->name, name) == 0) {
            return (char *)buffer->data + entry->offset;
        }
    }
    RAXEL_CORE_LOG_ERROR("Field '%s' not found in storage buffer\n", name);
    return NULL;
}

void raxel_sb_buffer_set(raxel_sb_buffer_t *buffer, char *name, void *data)
{
    for (raxel_size_t i = 0; i < raxel_array_size(buffer->entries); i++) {
        raxel_sb_entry_t *entry = &buffer->entries[i];
        if (strcmp(entry->name, name) == 0) {
            memcpy((char *)buffer->data + entry->offset, data, entry->size);
            return;
        }
    }
    RAXEL_CORE_LOG_ERROR("Field '%s' not found in storage buffer\n", name);
}

void raxel_sb_buffer_update(raxel_sb_buffer_t *buffer, raxel_pipeline_t *pipeline) {
    void *mapped = NULL;
    VkDevice device = pipeline->resources.device;
    VK_CHECK(vkMapMemory(device, buffer->memory, 0, buffer->data_size, 0, &mapped));
    memcpy(mapped, buffer->data, buffer->data_size);
    vkUnmapMemory(device, buffer->memory);
}


void raxel_sb_buffer_destroy(raxel_sb_buffer_t *buffer, VkDevice device)
{
    raxel_list_destroy(buffer->entries);
    raxel_free(buffer->allocator, buffer->data);
    vkDestroyBuffer(device, buffer->buffer, NULL);
    vkFreeMemory(device, buffer->memory, NULL);
    raxel_free(buffer->allocator, buffer);
}
