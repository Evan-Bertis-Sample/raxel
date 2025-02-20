#ifndef __RAXEL_HASHTABLE_H__
#define __RAXEL_HASHTABLE_H__

#include <stdint.h>

#include "raxel_container.h"  // for raxel_size_t
#include "raxel_mem.h"        // for raxel_allocator_t and raxel_size_t

#ifdef __cplusplus
extern "C" {
#endif

typedef struct raxel_hashtable {
    raxel_size_t __capacity;    // total number of buckets
    raxel_size_t __size;        // number of active entries
    raxel_size_t __key_size;    // size of a key in bytes
    raxel_size_t __value_size;  // size of a value in bytes
    raxel_allocator_t *__allocator;
    uint64_t (*__hash)(const void *key, raxel_size_t key_size);
    int (*__equals)(const void *key1, const void *key2, raxel_size_t key_size);
    void *__buckets;             // contiguous bucket memory
    raxel_size_t __bucket_size;  // computed as: 1 + __key_size + __value_size
} raxel_hashtable_t;

/**
 * Creates a new hashtable.
 * - allocator: the allocator to use.
 * - initial_capacity: number of buckets to allocate initially.
 * - key_size, value_size: sizes of key and value types.
 * - hash: (optional) custom hash function. If NULL, a default FNV-1a hash is used.
 * - equals: (optional) custom equality function. If NULL, memcmp is used.
 */
raxel_hashtable_t *__raxel_hashtable_create(raxel_allocator_t *allocator,
                                               raxel_size_t initial_capacity,
                                               raxel_size_t key_size,
                                               raxel_size_t value_size,
                                               uint64_t (*hash)(const void *, raxel_size_t),
                                               int (*equals)(const void *, const void *, raxel_size_t));

/** Destroys the hashtable and frees all associated memory. */
void raxel_hashtable_destroy(raxel_hashtable_t *ht);

/**
 * Inserts a key/value pair into the hashtable.
 * If the key already exists, its value is updated.
 * Returns 1 if a new key was inserted, 0 if an existing key was updated.
 */
int raxel_hashtable_insert(raxel_hashtable_t *ht, const void *key, const void *value);

/**
 * Looks up the key in the hashtable.
 * If found, copies the value into value_out and returns 1; otherwise returns 0.
 */
int raxel_hashtable_get(raxel_hashtable_t *ht, const void *key, void *value_out);

/**
 * Removes a key from the hashtable.
 * Returns 1 if the key was found and removed, 0 if not found.
 */
int raxel_hashtable_remove(raxel_hashtable_t *ht, const void *key);

#ifdef __cplusplus
}
#endif

/**
 * Convenience macros:
 * raxel_hashtable_create(key_type, value_type, allocator, initial_capacity)
 * creates a hashtable for the given key and value types using default hash and equals.
 */
#define raxel_hashtable_create(key_type, value_type, allocator, initial_capacity) \
    __raxel_hashtable_create(allocator, initial_capacity, sizeof(key_type), sizeof(value_type), NULL, NULL)

/**
 * raxel_hashtable_create_custom(key_type, value_type, allocator, initial_capacity, hash_func, equals_func)
 * creates a hashtable using custom hash and equals functions.
 */
#define raxel_hashtable_create_custom(key_type, value_type, allocator, initial_capacity, hash_func, equals_func) \
    __raxel_hashtable_create(allocator, initial_capacity, sizeof(key_type), sizeof(value_type), hash_func, equals_func)

#endif  // __RAXEL_HASHTABLE_H__
