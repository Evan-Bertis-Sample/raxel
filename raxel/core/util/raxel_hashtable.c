#include "raxel_hashtable.h"

#include <stdlib.h>
#include <string.h>

#include "raxel_mem.h"

/*---------------------------------------------------------------
  Helper macros and inline functions for bucket access.
---------------------------------------------------------------*/

// Each bucket is laid out as:
// [ state (1 byte) ][ key (key_size bytes) ][ value (value_size bytes) ]
// State values: 0 = empty, 1 = occupied, 2 = tombstone

// Returns pointer to bucket i.
static inline void *raxel_ht_get_bucket(raxel_hashtable_t *ht, raxel_size_t index) {
    return (void *)((char *)ht->__buckets + index * ht->__bucket_size);
}

// Returns pointer to the state flag of the bucket.
static inline uint8_t *raxel_ht_bucket_state(void *bucket) {
    return (uint8_t *)bucket;
}

// Returns pointer to the key within the bucket.
static inline void *raxel_ht_bucket_key(void *bucket) {
    return (void *)((char *)bucket + 1);
}

// Returns pointer to the value within the bucket.
static inline void *raxel_ht_bucket_value(void *bucket, raxel_hashtable_t *ht) {
    return (void *)((char *)bucket + 1 + ht->__key_size);
}

/*---------------------------------------------------------------
  Default hash and equality functions.
---------------------------------------------------------------*/
static uint64_t raxel_default_hash(const void *key, raxel_size_t key_size) {
    // FNV-1a 64-bit hash.
    uint64_t hash = 1469598103934665603ULL;
    const unsigned char *p = (const unsigned char *)key;
    for (raxel_size_t i = 0; i < key_size; i++) {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int raxel_default_equals(const void *key1, const void *key2, raxel_size_t key_size) {
    return memcmp(key1, key2, key_size) == 0;
}

/*---------------------------------------------------------------
  Rehashing.
---------------------------------------------------------------*/
static void raxel_hashtable_rehash(raxel_hashtable_t *ht, raxel_size_t new_capacity) {
    raxel_size_t new_bucket_size = 1 + ht->__key_size + ht->__value_size;
    void *new_buckets = raxel_malloc(ht->__allocator, new_capacity * new_bucket_size);
    memset(new_buckets, 0, new_capacity * new_bucket_size);  // all buckets empty

    // Reinsert existing entries.
    for (raxel_size_t i = 0; i < ht->__capacity; i++) {
        void *old_bucket = raxel_ht_get_bucket(ht, i);
        if (*raxel_ht_bucket_state(old_bucket) == 1) {  // occupied
            void *old_key = raxel_ht_bucket_key(old_bucket);
            void *old_value = raxel_ht_bucket_value(old_bucket, ht);
            uint64_t hash = ht->__hash(old_key, ht->__key_size);
            raxel_size_t index = hash % new_capacity;
            for (;;) {
                void *new_bucket = (void *)((char *)new_buckets + index * new_bucket_size);
                uint8_t *state = (uint8_t *)new_bucket;
                if (*state != 1) {  // empty or tombstone
                    *state = 1;
                    memcpy((char *)new_bucket + 1, old_key, ht->__key_size);
                    memcpy((char *)new_bucket + 1 + ht->__key_size, old_value, ht->__value_size);
                    break;
                }
                index = (index + 1) % new_capacity;
            }
        }
    }
    raxel_free(ht->__allocator, ht->__buckets);
    ht->__buckets = new_buckets;
    ht->__capacity = new_capacity;
    ht->__bucket_size = new_bucket_size;
}

/*---------------------------------------------------------------
  Public API.
---------------------------------------------------------------*/
raxel_hashtable_t *__raxel_hashtable_create(raxel_allocator_t *allocator,
                                            raxel_size_t initial_capacity,
                                            raxel_size_t key_size,
                                            raxel_size_t value_size,
                                            uint64_t (*hash)(const void *, raxel_size_t),
                                            int (*equals)(const void *, const void *, raxel_size_t)) {
    if (initial_capacity == 0) {
        initial_capacity = 8;  // default minimum capacity
    }
    raxel_hashtable_t *ht = (raxel_hashtable_t *)raxel_malloc(allocator, sizeof(raxel_hashtable_t));
    ht->__capacity = initial_capacity;
    ht->__size = 0;
    ht->__key_size = key_size;
    ht->__value_size = value_size;
    ht->__allocator = allocator;
    ht->__hash = hash ? hash : raxel_default_hash;
    ht->__equals = equals ? equals : raxel_default_equals;
    ht->__bucket_size = 1 + key_size + value_size;
    ht->__buckets = raxel_malloc(allocator, ht->__capacity * ht->__bucket_size);
    memset(ht->__buckets, 0, ht->__capacity * ht->__bucket_size);
    return ht;
}

void raxel_hashtable_destroy(raxel_hashtable_t *ht) {
    if (!ht) return;
    if (ht->__buckets) {
        raxel_free(ht->__allocator, ht->__buckets);
    }
    raxel_free(ht->__allocator, ht);
}

int raxel_hashtable_insert(raxel_hashtable_t *ht, const void *key, const void *value) {
    // Rehash if load factor >= 70%.
    if (ht->__size * 100 / ht->__capacity >= 70) {
        raxel_hashtable_rehash(ht, ht->__capacity * 2);
    }
    uint64_t hash = ht->__hash(key, ht->__key_size);
    raxel_size_t index = hash % ht->__capacity;
    for (;;) {
        void *bucket = (void *)((char *)ht->__buckets + index * ht->__bucket_size);
        uint8_t *state = (uint8_t *)bucket;
        if (*state == 1) {
            // Bucket occupied—check for key equality.
            void *bucket_key = raxel_ht_bucket_key(bucket);
            if (ht->__equals(key, bucket_key, ht->__key_size)) {
                // Key exists; update value.
                void *bucket_value = raxel_ht_bucket_value(bucket, ht);
                memcpy(bucket_value, value, ht->__value_size);
                return 0;  // updated
            }
        } else {
            // Found empty bucket (or tombstone).
            *state = 1;
            memcpy((char *)bucket + 1, key, ht->__key_size);
            memcpy((char *)bucket + 1 + ht->__key_size, value, ht->__value_size);
            ht->__size++;
            return 1;  // new insertion
        }
        index = (index + 1) % ht->__capacity;
    }
}

int raxel_hashtable_get(raxel_hashtable_t *ht, const void *key, void *value_out) {
    uint64_t hash = ht->__hash(key, ht->__key_size);
    raxel_size_t index = hash % ht->__capacity;
    for (;;) {
        void *bucket = (void *)((char *)ht->__buckets + index * ht->__bucket_size);
        uint8_t state = *(uint8_t *)bucket;
        if (state == 0) {
            // Never used: key not found.
            return 0;
        } else if (state == 1) {
            void *bucket_key = raxel_ht_bucket_key(bucket);
            if (ht->__equals(key, bucket_key, ht->__key_size)) {
                void *bucket_value = raxel_ht_bucket_value(bucket, ht);
                memcpy(value_out, bucket_value, ht->__value_size);
                return 1;
            }
        }
        index = (index + 1) % ht->__capacity;
    }
}

int raxel_hashtable_remove(raxel_hashtable_t *ht, const void *key) {
    uint64_t hash = ht->__hash(key, ht->__key_size);
    raxel_size_t index = hash % ht->__capacity;
    for (;;) {
        void *bucket = (void *)((char *)ht->__buckets + index * ht->__bucket_size);
        uint8_t *state = (uint8_t *)bucket;
        if (*state == 0) {
            // Not found.
            return 0;
        } else if (*state == 1) {
            void *bucket_key = raxel_ht_bucket_key(bucket);
            if (ht->__equals(key, bucket_key, ht->__key_size)) {
                // Mark as tombstone.
                *state = 2;
                ht->__size--;
                return 1;
            }
        }
        index = (index + 1) % ht->__capacity;
    }
}

static void *ht_it_current(raxel_iterator_t *it) {
    raxel_hashtable_t *ht = (raxel_hashtable_t *)it->__ctx;
    uintptr_t idx = (uintptr_t)it->__data;
    if (idx >= ht->__capacity) {
        return NULL;
    }
    void *bucket = raxel_ht_get_bucket(ht, idx);
    if (*(uint8_t *)bucket == 1) {
        return bucket;
    }
    return NULL;
}

static void *ht_it_next(raxel_iterator_t *it) {
    raxel_hashtable_t *ht = (raxel_hashtable_t *)it->__ctx;
    uintptr_t idx = (uintptr_t)it->__data;
    idx++;  // move to the next bucket
    while (idx < ht->__capacity) {
        void *bucket = raxel_ht_get_bucket(ht, idx);
        if (*(uint8_t *)bucket == 1) {  // found an occupied bucket
            it->__data = (void *)(uintptr_t)idx;
            return bucket;
        }
        idx++;
    }
    // If no more entries, set iterator index to capacity and return NULL.
    it->__data = (void *)(uintptr_t)ht->__capacity;
    return NULL;
}

raxel_iterator_t raxel_hashtable_iterator(raxel_hashtable_t *ht) {
    raxel_iterator_t it;
    it.__ctx = ht;
    // Start at the first occupied bucket.
    uintptr_t idx = 0;
    while (idx < ht->__capacity) {
        void *bucket = raxel_ht_get_bucket(ht, idx);
        if (*(uint8_t *)bucket == 1) {
            break;
        }
        idx++;
    }
    it.__data = (void *)(uintptr_t)idx;
    it.current = ht_it_current;
    it.next = ht_it_next;
    return it;
}