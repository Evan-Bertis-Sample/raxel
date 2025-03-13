#include "raxel_sb_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        uint32_t fieldEnd = desc->entries[i].offset + desc->entries[i].size;
        if (fieldEnd > buffer->data_size) {
            buffer->data_size = fieldEnd;
        }
    }
    // Allocate CPU–side memory.
    buffer->data = raxel_malloc(allocator, buffer->data_size);

    // --- Create the Vulkan buffer ---
    VkBufferCreateInfo bufInfo = {0};
    bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufInfo.size = buffer->data_size;
    bufInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device, &bufInfo, NULL, &buffer->buffer));

    // Query memory requirements.
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(device, buffer->buffer, &memReq);

    // Find a memory type that is host-visible and coherent.
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    uint32_t memoryTypeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((memReq.memoryTypeBits & (1 << i)) &&
            (memProps.memoryTypes[i].propertyFlags &
             (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) ==
                (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
        {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == UINT32_MAX) {
        fprintf(stderr, "Failed to find suitable memory type for storage buffer.\n");
        exit(EXIT_FAILURE);
    }

    VkMemoryAllocateInfo allocInfo = {0};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    VK_CHECK(vkAllocateMemory(device, &allocInfo, NULL, &buffer->memory));
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

void raxel_sb_buffer_destroy(raxel_sb_buffer_t *buffer, VkDevice device)
{
    raxel_list_destroy(buffer->entries);
    raxel_free(buffer->allocator, buffer->data);
    vkDestroyBuffer(device, buffer->buffer, NULL);
    vkFreeMemory(device, buffer->memory, NULL);
    raxel_free(buffer->allocator, buffer);
}
