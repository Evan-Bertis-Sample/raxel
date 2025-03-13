#ifndef __RAXEL_SB_BUFFER_H__
#define __RAXEL_SB_BUFFER_H__

#include <raxel/core/graphics/vk.h>
#include <raxel/core/util.h>

typedef struct raxel_sb_entry {
    char *name;       // Field name
    uint32_t offset;  // Offset into the storage buffer
    uint32_t size;    // Size (in bytes) of this field
} raxel_sb_entry_t;

typedef struct raxel_sb_buffer_desc {
    raxel_sb_entry_t *entries;  // Array of entries
    raxel_size_t entry_count;   // Number of entries in the array
} raxel_sb_buffer_desc_t;

typedef struct raxel_sb_buffer {
    raxel_array(raxel_sb_entry_t) entries;  // Array of entries
    void *data;                             // CPU–side data mirror
    raxel_size_t data_size;                 // Total size of the data buffer
    raxel_allocator_t *allocator;           // Allocator used for memory

    VkBuffer buffer;        // Vulkan buffer handle
    VkDeviceMemory memory;  // Device memory bound to the buffer
} raxel_sb_buffer_t;

#define RAXEL_SB_DESC(...)                                                                  \
    (raxel_sb_buffer_desc_t) {                                                              \
        .entries = (raxel_sb_entry_t[]){__VA_ARGS__},                                       \
        .entry_count = sizeof((raxel_sb_entry_t[]){__VA_ARGS__}) / sizeof(raxel_sb_entry_t) \
    }

/**
 * Create a storage buffer from a descriptor.
 *
 * @param allocator The allocator to use.
 * @param desc Descriptor that defines the layout and total size.
 * @param device Vulkan device.
 * @param physicalDevice Vulkan physical device.
 * @return A pointer to the new storage buffer.
 */
raxel_sb_buffer_t *raxel_sb_buffer_create(raxel_allocator_t *allocator,
                                          raxel_sb_buffer_desc_t *desc,
                                          VkDevice device,
                                          VkPhysicalDevice physicalDevice);

/**
 * Get a pointer to a field in the storage buffer by name.
 */
void *raxel_sb_buffer_get(raxel_sb_buffer_t *buffer, char *name);

/**
 * Write data into the storage buffer for a given field name.
 *
 * @param buffer The storage buffer.
 * @param name Field name.
 * @param data Pointer to the data to copy (must match the field’s size).
 */
void raxel_sb_buffer_set(raxel_sb_buffer_t *buffer, char *name, void *data);

/**
 * Destroy the storage buffer.
 *
 * @param buffer The storage buffer.
 * @param device Vulkan device.
 */
void raxel_sb_buffer_destroy(raxel_sb_buffer_t *buffer, VkDevice device);

#endif  // __RAXEL_SB_BUFFER_H__
