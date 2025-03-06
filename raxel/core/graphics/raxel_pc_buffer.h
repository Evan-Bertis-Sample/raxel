#ifndef __RAXEL_PC_BUFFER_H__
#define __RAXEL_PC_BUFFER_H__

#include <raxel/core/util.h>

typedef struct raxel_pc_entry {
    char *name;       // Field name
    uint32_t offset;  // Offset into the push constant buffer
    uint32_t size;    // Size (in bytes) of this field
} raxel_pc_entry_t;

typedef struct raxel_pc_buffer_desc {
    raxel_pc_entry_t *entries;  // Array of entries
    raxel_size_t entry_count;   // Number of entries in the array
} raxel_pc_buffer_desc_t;

typedef struct raxel_pc_buffer {
    raxel_array(raxel_pc_entry_t) entries;  // Array of entries
    void *data;                             // Pointer to the allocated data block
    raxel_size_t data_size;                 // Total size of the data buffer
    raxel_allocator_t *allocator;           // Allocator used for memory
} raxel_pc_buffer_t;

#define RAXEL_PC_DESC(...)                                                                  \
    (raxel_pc_buffer_desc_t) {                                                                     \
        .entries = (raxel_pc_entry_t[]){__VA_ARGS__},                                       \
        .entry_count = sizeof((raxel_pc_entry_t[]){__VA_ARGS__}) / sizeof(raxel_pc_entry_t) \
    }

/**
 * Create a push–constant buffer from a descriptor.
 *
 * @param allocator The allocator to use.
 * @param desc Descriptor that defines the layout and total size.
 * @return A pointer to the new push–constant buffer.
 */
raxel_pc_buffer_t *raxel_pc_buffer_create(raxel_allocator_t *allocator, raxel_pc_buffer_desc_t *desc);

void *raxel_pc_buffer_get(raxel_pc_buffer_t *buffer, char *name);

/**
 * Write data into the push–constant buffer for a given field name.
 *
 * @param buffer The push–constant buffer.
 * @param name Field name.
 * @param data Pointer to the data to copy (must be exactly the field's size).
 */
void raxel_pc_buffer_set(raxel_pc_buffer_t *buffer, char *name, void *data);

/**
 * Destroy the push–constant buffer.
 *
 * @param buffer The push–constant buffer.
 */
void raxel_pc_buffer_destroy(raxel_pc_buffer_t *buffer);

#endif  // __RAXEL_PC_BUFFER_H__
