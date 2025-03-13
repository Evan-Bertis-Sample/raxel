#include "raxel_pc_buffer.h"

#include <raxel/core/util.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


raxel_pc_buffer_t *raxel_pc_buffer_create(raxel_allocator_t *allocator, raxel_pc_buffer_desc_t *desc) {
    raxel_pc_buffer_t *buffer = raxel_malloc(allocator, sizeof(raxel_pc_buffer_t));
    buffer->allocator = allocator;

    buffer->entries = raxel_array_create(raxel_pc_entry_t, allocator, desc->entry_count);

    // Calculate the total size of the data buffer.
    buffer->data_size = 0;
    for (raxel_size_t i = 0; i < desc->entry_count; i++) {
        raxel_size_t new_size = desc->entries[i].offset + desc->entries[i].size;
        buffer->data_size = new_size > buffer->data_size ? new_size : buffer->data_size;

        raxel_pc_entry_t entry = desc->entries[i];
        buffer->entries[i] = entry;
    }

    RAXEL_CORE_LOG("Allocating push-constant buffer of size %u\n", buffer->data_size);

    buffer->data = raxel_malloc(allocator, buffer->data_size);

    return buffer;
}

void *raxel_pc_buffer_get(raxel_pc_buffer_t *buffer, char *name) {
    for (raxel_size_t i = 0; i < raxel_array_size(buffer->entries); i++) {
        raxel_pc_entry_t *entry = &buffer->entries[i];
        if (strcmp(entry->name, name) == 0) {
            return (char *)buffer->data + entry->offset;
        }
    }
    RAXEL_CORE_LOG_ERROR("Field '%s' not found in push-constant buffer\n", name);
    return NULL;
}

void raxel_pc_buffer_set(raxel_pc_buffer_t *buffer, char *name, void *data) {
    for (raxel_size_t i = 0; i < raxel_array_size(buffer->entries); i++) {
        raxel_pc_entry_t *entry = &buffer->entries[i];
        if (strcmp(entry->name, name) == 0) {
            memcpy((char *)buffer->data + entry->offset, data, entry->size);
            return;
        }
    }
    RAXEL_CORE_LOG_ERROR("Field '%s' not found in push-constant buffer\n", name);
}

void raxel_pc_buffer_destroy(raxel_pc_buffer_t *buffer) {
    raxel_list_destroy(buffer->entries);
    raxel_free(buffer->allocator, buffer->data);
    raxel_free(buffer->allocator, buffer);
}
