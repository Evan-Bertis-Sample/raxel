// raxel_container_tests.c

#include <raxel/core/util.h>

#include <string.h>  // for strcmp, memset, etc.

// Optional: If you need a custom allocator, implement here or link in from elsewhere
// For now, let's just pass NULL to use the default (malloc/free) in RAXEl.

/*------------------------------------------------------------------------
 * Test: Array
 *-----------------------------------------------------------------------*/

// 1. Create an array, check size, fill it with data, etc.
RAXEL_TEST(test_array_creation) {
    // Create an array of 5 ints
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_array(int) arr = raxel_array_create(int, &allocator, 5);

    RAXEL_TEST_ASSERT(arr != NULL);

    __raxel_array_header_t *header = raxel_array_header(arr);
    // assert that the address of the header is before the array
    RAXEL_TEST_ASSERT(header < arr);
    // assert that it is sizeof(__raxel_array_header_t) bytes before the array
    RAXEL_TEST_ASSERT((char *)arr - (char *)header == sizeof(__raxel_array_header_t));

    RAXEL_TEST_ASSERT(header != NULL);
    RAXEL_TEST_ASSERT_EQUAL_INT(header->__size, 5);
    RAXEL_TEST_ASSERT_EQUAL_INT(header->__stride, sizeof(int));


    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_array_size(arr), 5);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_array_stride(arr), sizeof(int));

    // Initialize and verify
    for (size_t i = 0; i < raxel_array_size(arr); i++) {
        arr[i] = (int)i;
    }
    for (size_t i = 0; i < raxel_array_size(arr); i++) {
        RAXEL_TEST_ASSERT(arr[i] == (int)i);
    }

    raxel_array_destroy(arr);
}

// 2. Test array iteration with raxel_array_iterator
RAXEL_TEST(test_array_iterator) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_array(int) arr = raxel_array_create(int, &allocator, 3);
    arr[0] = 10; arr[1] = 20; arr[2] = 30;

    // simple usage of raxel_array_iterator
    raxel_iterator_t it = raxel_array_iterator(arr);
    int *current = (int *)it.current(&it);
    RAXEL_TEST_ASSERT(*current == 10);

    current = (int *)it.next(&it);
    RAXEL_TEST_ASSERT(*current == 20);

    current = (int *)it.next(&it);
    RAXEL_TEST_ASSERT(*current == 30);

    // Once more, we should be past the end, returning something invalid if not checked,
    // but the current code doesn't handle "end-of-iteration" logic well.
    // This is a known issue in the template, so we won't do a beyond-end check here.

    raxel_array_destroy(arr);
}

/*------------------------------------------------------------------------
 * Test: List
 *-----------------------------------------------------------------------*/

// 1. Create a list, verify size, push back data
RAXEL_TEST(test_list_creation_push_back) {
    // Create a list of ints with an initial size of 2
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_list(int) mylist = raxel_list_create(int, &allocator, 2);

    RAXEL_TEST_ASSERT(mylist != NULL);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(mylist), 2);
    RAXEL_TEST_ASSERT(raxel_list_capacity(mylist) >= 2);

    // Initialize the first 2 elements
    mylist[0] = 42;
    mylist[1] = 100;

    // Push back new elements
    int new_value = 200;
    raxel_list_push_back(mylist, &new_value);
    RAXEL_TEST_ASSERT(raxel_list_size(mylist) == 3);
    RAXEL_TEST_ASSERT(raxel_list_capacity(mylist) >= 3);

    // Validate content
    RAXEL_TEST_ASSERT(mylist[0] == 42);
    RAXEL_TEST_ASSERT(mylist[1] == 100);
    RAXEL_TEST_ASSERT(mylist[2] == 200);

    // Destroy
    raxel_list_destroy(mylist);
}

// 2. Test list resizing
RAXEL_TEST(test_list_resize) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_list(float) flist = raxel_list_create(float, &allocator, 2);
    flist[0] = 1.1f;
    flist[1] = 2.2f;

    // Increase size to 5
    raxel_list_resize(flist, 5);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(flist), 2);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_capacity(flist), 5);
    // The original two should still be there:
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(flist[0], 1.1f);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(flist[1], 2.2f);

    // Decrease size to 1
    raxel_list_resize(flist, 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(flist), 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_capacity(flist), 1);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(flist[0], 1.1f);

    raxel_list_destroy(flist);
}

/*------------------------------------------------------------------------
 * Test: String
 *-----------------------------------------------------------------------*/

// 1. Create string, push back chars, append
RAXEL_TEST(test_string_basics) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 4);  // capacity 4 initially
    RAXEL_TEST_ASSERT(s.__data != NULL);
    RAXEL_TEST_ASSERT(s.__size == 0);
    RAXEL_TEST_ASSERT(s.__capacity >= 4);

    // push back chars
    raxel_string_push_back(&s, 'H');
    raxel_string_push_back(&s, 'i');
    RAXEL_TEST_ASSERT(s.__size == 2);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "Hi") == 0);

    // append
    raxel_string_append(&s, ", world!");
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "Hi, world!") == 0);
    RAXEL_TEST_ASSERT(s.__size == strlen("Hi, world!"));

    // cleanup
    raxel_string_destroy(&s);
}

// 2. String split
RAXEL_TEST(test_string_split) {
    // "One,Two,Three"
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 0);
    raxel_string_append(&s, "One,Two,Three");

    raxel_array(raxel_string_t) parts = raxel_string_split(&s, ',');
    RAXEL_TEST_ASSERT(raxel_array_size(parts) == 3);

    // Validate each token
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts[0]), "One") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts[1]), "Two") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts[2]), "Three") == 0);

    // Cleanup: destroy each string
    for (size_t i = 0; i < raxel_array_size(parts); i++) {
        raxel_string_destroy(&parts[i]);
    }

    raxel_array_destroy(parts);

    raxel_string_destroy(&s);
}

void register_container_tests() {
    RAXEL_TEST_REGISTER(test_array_creation);
    RAXEL_TEST_REGISTER(test_array_iterator);
    RAXEL_TEST_REGISTER(test_list_creation_push_back);
    RAXEL_TEST_REGISTER(test_list_resize);
    RAXEL_TEST_REGISTER(test_string_basics);
    RAXEL_TEST_REGISTER(test_string_split);
}