#include <raxel/core/util.h>
#include <string.h>

/*------------------------------------------------------------
  Custom structures for keys and values.
------------------------------------------------------------*/
typedef struct {
    int id;
    char name[32];
} custom_key_t;

typedef struct {
    double score;
    int rank;
} custom_value_t;

/*------------------------------------------------------------
  Custom hash and equals for custom_key_t.
------------------------------------------------------------*/
static uint64_t custom_key_hash(const void *key, raxel_size_t key_size) {
    (void)key_size;  // we know the structure's layout
    const custom_key_t *ck = (const custom_key_t *)key;
    uint64_t hash = 1469598103934665603ULL;  // FNV-1a 64-bit offset basis

    // Incorporate the integer id.
    const unsigned char *p = (const unsigned char *)&ck->id;
    for (size_t i = 0; i < sizeof(int); i++) {
        hash ^= p[i];
        hash *= 1099511628211ULL;
    }
    // Incorporate the name.
    p = (const unsigned char *)ck->name;
    while (*p) {
        hash ^= *p;
        hash *= 1099511628211ULL;
        p++;
    }
    return hash;
}

static int custom_key_equals(const void *a, const void *b, raxel_size_t key_size) {
    (void)key_size;
    const custom_key_t *ak = (const custom_key_t *)a;
    const custom_key_t *bk = (const custom_key_t *)b;
    if (ak->id != bk->id)
        return 0;
    return strcmp(ak->name, bk->name) == 0;
}

/*------------------------------------------------------------
  Test: Basic insertion, lookup, and update.
------------------------------------------------------------*/
RAXEL_TEST(test_hashtable_basic) {
    raxel_allocator_t allocator = raxel_default_allocator();
    // Create a hashtable mapping int keys to int values.
    raxel_hashtable_t *ht = raxel_hashtable_create(int, int, &allocator, 8);

    int key = 42;
    int value = 100;
    int ret = raxel_hashtable_insert(ht, &key, &value);
    RAXEL_TEST_ASSERT(ret == 1);  // New insertion.

    int got = 0;
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(got, 100);

    // Update the existing key.
    value = 200;
    ret = raxel_hashtable_insert(ht, &key, &value);
    RAXEL_TEST_ASSERT(ret == 0);  // Updated existing key.
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(got, 200);

    raxel_hashtable_destroy(ht);
}

/*------------------------------------------------------------
  Test: Removal of keys.
------------------------------------------------------------*/
RAXEL_TEST(test_hashtable_remove) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_hashtable_t *ht = raxel_hashtable_create(int, int, &allocator, 8);

    int key1 = 10, key2 = 20, key3 = 30;
    int value1 = 100, value2 = 200, value3 = 300;
    raxel_hashtable_insert(ht, &key1, &value1);
    raxel_hashtable_insert(ht, &key2, &value2);
    raxel_hashtable_insert(ht, &key3, &value3);

    // Remove key2.
    RAXEL_TEST_ASSERT(raxel_hashtable_remove(ht, &key2) == 1);
    int got = 0;
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key2, &got) == 0);

    // Ensure remaining keys are still accessible.
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key1, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(got, 100);
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key3, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(got, 300);

    // Removing a non-existing key returns 0.
    int key4 = 40;
    RAXEL_TEST_ASSERT(raxel_hashtable_remove(ht, &key4) == 0);

    raxel_hashtable_destroy(ht);
}

/*------------------------------------------------------------
  Test: Rehashing by inserting many entries.
------------------------------------------------------------*/
RAXEL_TEST(test_hashtable_rehash) {
    raxel_allocator_t allocator = raxel_default_allocator();
    // Start with a small capacity to force rehashing.
    raxel_hashtable_t *ht = raxel_hashtable_create(int, int, &allocator, 4);
    const int num_entries = 50;
    for (int i = 0; i < num_entries; i++) {
        int key = i;
        int value = i * 10;
        raxel_hashtable_insert(ht, &key, &value);
    }
    for (int i = 0; i < num_entries; i++) {
        int key = i;
        int got = 0;
        RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key, &got) == 1);
        RAXEL_TEST_ASSERT_EQUAL_INT(got, i * 10);
    }
    raxel_hashtable_destroy(ht);
}

/*------------------------------------------------------------
  Test: Custom hash function (forcing collisions).
------------------------------------------------------------*/
static uint64_t constant_hash(const void *key, raxel_size_t key_size) {
    (void)key;
    (void)key_size;
    return 42;  // constant hash forces collisions.
}

RAXEL_TEST(test_hashtable_custom_hash) {
    raxel_allocator_t allocator = raxel_default_allocator();
    // Create a hashtable using the custom constant hash to force collisions.
    raxel_hashtable_t *ht = raxel_hashtable_create_custom(int, int, &allocator, 8, constant_hash, NULL);
    for (int i = 0; i < 10; i++) {
        int key = i;
        int value = i + 100;
        raxel_hashtable_insert(ht, &key, &value);
    }
    for (int i = 0; i < 10; i++) {
        int key = i;
        int got = 0;
        RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key, &got) == 1);
        RAXEL_TEST_ASSERT_EQUAL_INT(got, i + 100);
    }
    raxel_hashtable_destroy(ht);
}

/*------------------------------------------------------------
  Test: Using custom structures for keys and values.
------------------------------------------------------------*/
RAXEL_TEST(test_hashtable_custom_structs) {
    raxel_allocator_t allocator = raxel_default_allocator();
    // Create a hashtable mapping custom_key_t to custom_value_t with custom hash and equals.
    raxel_hashtable_t *ht = raxel_hashtable_create_custom(custom_key_t, custom_value_t, &allocator, 8,
                                                          custom_key_hash, custom_key_equals);

    custom_key_t key1 = {.id = 1};
    strcpy(key1.name, "Alice");
    custom_value_t value1 = {.score = 95.5, .rank = 1};

    custom_key_t key2 = {.id = 2};
    strcpy(key2.name, "Bob");
    custom_value_t value2 = {.score = 87.0, .rank = 2};

    custom_key_t key3 = {.id = 3};
    strcpy(key3.name, "Charlie");
    custom_value_t value3 = {.score = 78.3, .rank = 3};

    // Insert entries.
    RAXEL_TEST_ASSERT(raxel_hashtable_insert(ht, &key1, &value1) == 1);
    RAXEL_TEST_ASSERT(raxel_hashtable_insert(ht, &key2, &value2) == 1);
    RAXEL_TEST_ASSERT(raxel_hashtable_insert(ht, &key3, &value3) == 1);

    // Lookup and verify.
    custom_value_t got;
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key1, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(got.score, value1.score);
    RAXEL_TEST_ASSERT_EQUAL_INT(got.rank, value1.rank);

    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key2, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(got.score, value2.score);
    RAXEL_TEST_ASSERT_EQUAL_INT(got.rank, value2.rank);

    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key3, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(got.score, value3.score);
    RAXEL_TEST_ASSERT_EQUAL_INT(got.rank, value3.rank);

    // Update key2.
    value2.score = 91.2;
    value2.rank = 2;
    RAXEL_TEST_ASSERT(raxel_hashtable_insert(ht, &key2, &value2) == 0);
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key2, &got) == 1);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(got.score, value2.score);
    RAXEL_TEST_ASSERT_EQUAL_INT(got.rank, value2.rank);

    // Remove key1.
    RAXEL_TEST_ASSERT(raxel_hashtable_remove(ht, &key1) == 1);
    RAXEL_TEST_ASSERT(raxel_hashtable_get(ht, &key1, &got) == 0);

    raxel_hashtable_destroy(ht);
}

RAXEL_TEST(test_hashtable_iterator) {
    raxel_allocator_t allocator = raxel_default_allocator();
    // Create a hashtable with capacity 8 for int->int mappings.
    raxel_hashtable_t *ht = raxel_hashtable_create(int, int, &allocator, 8);

    const int num_entries = 10;
    // Insert key i with value (i * 10) for 0 <= i < num_entries.
    for (int i = 0; i < num_entries; i++) {
        int value = i * 10;
        raxel_hashtable_insert(ht, &i, &value);
    }

    // Use the iterator to count and check all entries.
    int count = 0;
    raxel_iterator_t it = raxel_hashtable_iterator(ht);
    while (1) {
        void *bucket = it.current(&it);
        if (!bucket) break;  // No more occupied buckets.
        int key, value;
        // Recall: each bucket layout is:
        // [ state (1 byte) ][ key (sizeof(int) bytes) ][ value (sizeof(int) bytes) ]
        memcpy(&key, (char *)bucket + 1, sizeof(int));
        memcpy(&value, (char *)bucket + 1 + ht->__key_size, sizeof(int));
        // Verify the mapping: value should equal key * 10.
        RAXEL_TEST_ASSERT_EQUAL_INT(value, key * 10);
        count++;
        it.next(&it);
    }
    // Check that we visited exactly num_entries buckets.
    RAXEL_TEST_ASSERT_EQUAL_INT(count, num_entries);

    raxel_hashtable_destroy(ht);
}

/*------------------------------------------------------------
  Registration of all tests.
------------------------------------------------------------*/
void register_hashtable_tests() {
    RAXEL_TEST_REGISTER(test_hashtable_basic);
    RAXEL_TEST_REGISTER(test_hashtable_remove);
    RAXEL_TEST_REGISTER(test_hashtable_rehash);
    RAXEL_TEST_REGISTER(test_hashtable_custom_hash);
    RAXEL_TEST_REGISTER(test_hashtable_custom_structs);
    RAXEL_TEST_REGISTER(test_hashtable_iterator);
}
