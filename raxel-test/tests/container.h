// raxel_container_tests.c

#include <raxel/core/util.h>
#include <string.h>  // for strcmp, memcpy, etc.

/*------------------------------------------------------------------------
 * Test: Array
 *-----------------------------------------------------------------------*/

// 1. Basic creation, header, and element access.
RAXEL_TEST(test_array_creation) {
    // Create an array of 5 ints
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_array(int) arr = raxel_array_create(int, &allocator, 5);
    RAXEL_TEST_ASSERT(arr != NULL);

    __raxel_array_header_t *header = raxel_array_header(arr);
    // The header should reside in memory immediately before the array data.
    RAXEL_TEST_ASSERT(header < arr);
    RAXEL_TEST_ASSERT((char *)arr - (char *)header == sizeof(__raxel_array_header_t));
    RAXEL_TEST_ASSERT(header != NULL);
    RAXEL_TEST_ASSERT_EQUAL_INT(header->__size, 5);
    RAXEL_TEST_ASSERT_EQUAL_INT(header->__stride, sizeof(int));

    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_array_size(arr), 5);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_array_stride(arr), sizeof(int));

    // Initialize and verify elements.
    for (size_t i = 0; i < raxel_array_size(arr); i++) {
        arr[i] = (int)i;
    }
    for (size_t i = 0; i < raxel_array_size(arr); i++) {
        RAXEL_TEST_ASSERT(arr[i] == (int)i);
    }

    raxel_array_destroy(arr);
}

// 2. Array iteration using the iterator API.
RAXEL_TEST(test_array_iterator) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_array(int) arr = raxel_array_create(int, &allocator, 3);
    arr[0] = 10; arr[1] = 20; arr[2] = 30;

    // Use iterator to access each element.
    raxel_iterator_t it = raxel_array_iterator(arr);
    int *current = (int *)it.current(&it);
    RAXEL_TEST_ASSERT(*current == 10);

    current = (int *)it.next(&it);
    RAXEL_TEST_ASSERT(*current == 20);

    current = (int *)it.next(&it);
    RAXEL_TEST_ASSERT(*current == 30);

    raxel_array_destroy(arr);
}

// 3. Random-access test: fill array with a pattern and iterate.
RAXEL_TEST(test_array_random_access) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_array(int) arr = raxel_array_create(int, &allocator, 10);
    // Fill array with squares of indices.
    for (size_t i = 0; i < raxel_array_size(arr); i++) {
        arr[i] = (int)(i * i);
    }
    // Iterate using the iterator.
    raxel_iterator_t it = raxel_array_iterator(arr);
    for (size_t i = 0; i < raxel_array_size(arr); i++) {
        int *val = (int *)it.current(&it);
        RAXEL_TEST_ASSERT(*val == (int)(i * i));
        it.next(&it);
    }
    raxel_array_destroy(arr);
}

// 4. Array of characters.
RAXEL_TEST(test_array_char) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_array(char) arr = raxel_array_create(char, &allocator, 6);
    const char *text = "Hello";
    memcpy(arr, text, 5);
    arr[5] = '\0';
    RAXEL_TEST_ASSERT(strcmp(arr, "Hello") == 0);
    raxel_array_destroy(arr);
}

/*------------------------------------------------------------------------
 * Test: List
 *-----------------------------------------------------------------------*/

// 1. Basic creation and push_back.
RAXEL_TEST(test_list_creation_push_back) {
    // Create a list of ints with an initial size of 2.
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_list(int) mylist = raxel_list_create(int, &allocator, 2);

    RAXEL_TEST_ASSERT(mylist != NULL);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(mylist), 0); // empty list
    RAXEL_TEST_ASSERT(raxel_list_capacity(mylist) >= 2);

    // Initialize the first two elements.
    raxel_list_push_back(mylist, 42);
    raxel_list_push_back(mylist, 100);

    // Push back a new element.
    int new_value = 200;
    raxel_list_push_back(mylist, new_value);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(mylist), 3);
    RAXEL_TEST_ASSERT(raxel_list_capacity(mylist) >= 3);

    // Validate content.
    RAXEL_TEST_ASSERT_EQUAL_INT(mylist[0], 42);
    RAXEL_TEST_ASSERT_EQUAL_INT(mylist[1], 100);
    RAXEL_TEST_ASSERT_EQUAL_INT(mylist[2], 200);

    raxel_list_destroy(mylist);
}

// 2. Resize list: increasing and then decreasing size.
RAXEL_TEST(test_list_resize) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_list(float) flist = raxel_list_create(float, &allocator, 2);
    flist[0] = 1.1f;
    flist[1] = 2.2f;

    // Increase size to 5.
    raxel_list_resize(flist, 5);
    // In this implementation, a resize reallocation sets capacity equal to new size.
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(flist), 2);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_capacity(flist), 5);
    // The original two elements should be preserved.
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(flist[0], 1.1f);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(flist[1], 2.2f);

    // Decrease size to 1.
    raxel_list_resize(flist, 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(flist), 1);
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_capacity(flist), 1);
    RAXEL_TEST_ASSERT_EQUAL_FLOAT(flist[0], 1.1f);

    raxel_list_destroy(flist);
}

// 3. Many push_back: add many elements to force multiple reallocations.
RAXEL_TEST(test_list_many_push_back) {
    raxel_allocator_t allocator = raxel_default_allocator();
    // Create a list with an initial size of 1.
    raxel_list(int) list = raxel_list_create(int, &allocator, 1);
    // Reset size to 0 to simulate an empty list.
    // raxel_list_resize(list, 0);
    const int count = 100;
    for (int i = 0; i < count; i++) {
        raxel_list_push_back(list, i);
    }
    RAXEL_TEST_ASSERT_EQUAL_INT(raxel_list_size(list), count);
    for (int i = 0; i < count; i++) {
        RAXEL_TEST_ASSERT_EQUAL_INT(list[i], i);
    }
    raxel_list_destroy(list);
}

// 4. List iterator: iterate through list elements.
RAXEL_TEST(test_list_iterator) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_list(int) list = raxel_list_create(int, &allocator, 5);
    for (int i = 0; i < 5; i++) {
        list[i] = i + 10;
    }
    // Push back one more element.
    raxel_list_push_back(list, 99);
    // Iterate over the list.
    raxel_iterator_t it = raxel_list_iterator(list);
    int idx = 0;
    while (idx < raxel_list_size(list)) {
        int *value = (int *)it.current(&it);
        if (idx < 5) {
            RAXEL_TEST_ASSERT(*value == idx + 10);
        } else {
            RAXEL_TEST_ASSERT(*value == 99);
        }
        it.next(&it);
        idx++;
    }
    raxel_list_destroy(list);
}

/*------------------------------------------------------------------------
 * Test: String
 *-----------------------------------------------------------------------*/

// 1. Basic string creation, push_back, and append.
RAXEL_TEST(test_string_basics) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 4);  // initial capacity 4
    RAXEL_TEST_ASSERT(s.__data != NULL);
    RAXEL_TEST_ASSERT(s.__size == 0);
    RAXEL_TEST_ASSERT(s.__capacity >= 4);

    // Push back characters.
    raxel_string_push_back(&s, 'H');
    raxel_string_push_back(&s, 'i');
    RAXEL_TEST_ASSERT(s.__size == 2);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "Hi") == 0);

    // Append additional text.
    raxel_string_append(&s, ", world!");
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "Hi, world!") == 0);
    RAXEL_TEST_ASSERT(s.__size == strlen("Hi, world!"));

    raxel_string_destroy(&s);
}

// 2. String split: normal comma-separated tokens.
RAXEL_TEST(test_string_split) {
    // "One,Two,Three"
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 0);
    raxel_string_append(&s, "One,Two,Three");

    raxel_array(raxel_string_t) parts = raxel_string_split(&s, ',');
    RAXEL_TEST_ASSERT(raxel_array_size(parts) == 3);

    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts[0]), "One") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts[1]), "Two") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts[2]), "Three") == 0);

    // Clean up each token.
    for (size_t i = 0; i < raxel_array_size(parts); i++) {
        raxel_string_destroy(&parts[i]);
    }
    raxel_array_destroy(parts);
    raxel_string_destroy(&s);
}

// 3. Test empty string and clear.
RAXEL_TEST(test_string_empty_and_clear) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 10);
    RAXEL_TEST_ASSERT(s.__size == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "") == 0);
    raxel_string_append(&s, "Test");
    RAXEL_TEST_ASSERT(s.__size == 4);
    raxel_string_clear(&s);
    RAXEL_TEST_ASSERT(s.__size == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "") == 0);
    raxel_string_destroy(&s);
}

// 4. Multiple appends to build a longer string.
RAXEL_TEST(test_string_multiple_appends) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 8);
    raxel_string_append(&s, "Hello");
    raxel_string_append(&s, " ");
    raxel_string_append(&s, "World");
    raxel_string_append(&s, "!");
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&s), "Hello World!") == 0);
    raxel_string_destroy(&s);
}

// 5. Test raxel_string_to_cstr for proper null-termination.
RAXEL_TEST(test_string_to_cstr) {
    raxel_allocator_t allocator = raxel_default_allocator();
    raxel_string_t s = raxel_string_create(&allocator, 5);
    raxel_string_append(&s, "Test");
    char *cstr = raxel_string_to_cstr(&s);
    RAXEL_TEST_ASSERT(cstr != NULL);
    RAXEL_TEST_ASSERT(strcmp(cstr, "Test") == 0);
    raxel_string_destroy(&s);
}

// 6. String split edge cases.
RAXEL_TEST(test_string_split_edge_cases) {
    raxel_allocator_t allocator = raxel_default_allocator();
    
    // Edge case 1: empty string.
    raxel_string_t s_empty = raxel_string_create(&allocator, 0);
    raxel_array(raxel_string_t) parts_empty = raxel_string_split(&s_empty, ',');
    // Expect one token: the empty string.
    RAXEL_TEST_ASSERT(raxel_array_size(parts_empty) == 1);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_empty[0]), "") == 0);
    raxel_string_destroy(&s_empty);
    raxel_array_destroy(parts_empty);
    
    // Edge case 2: no delimiter present.
    raxel_string_t s_nodelem = raxel_string_create(&allocator, 0);
    raxel_string_append(&s_nodelem, "HelloWorld");
    raxel_array(raxel_string_t) parts_nodelem = raxel_string_split(&s_nodelem, ',');
    RAXEL_TEST_ASSERT(raxel_array_size(parts_nodelem) == 1);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_nodelem[0]), "HelloWorld") == 0);
    raxel_string_destroy(&s_nodelem);
    raxel_array_destroy(parts_nodelem);
    
    // Edge case 3: leading delimiter.
    raxel_string_t s_lead = raxel_string_create(&allocator, 0);
    raxel_string_append(&s_lead, ",Hello,World");
    raxel_array(raxel_string_t) parts_lead = raxel_string_split(&s_lead, ',');
    RAXEL_TEST_ASSERT(raxel_array_size(parts_lead) == 3);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_lead[0]), "") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_lead[1]), "Hello") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_lead[2]), "World") == 0);
    raxel_string_destroy(&s_lead);
    for (size_t i = 0; i < raxel_array_size(parts_lead); i++) {
        raxel_string_destroy(&parts_lead[i]);
    }
    raxel_array_destroy(parts_lead);
    
    // Edge case 4: trailing delimiter.
    raxel_string_t s_trail = raxel_string_create(&allocator, 0);
    raxel_string_append(&s_trail, "Hello,World,");
    raxel_array(raxel_string_t) parts_trail = raxel_string_split(&s_trail, ',');
    RAXEL_TEST_ASSERT(raxel_array_size(parts_trail) == 3);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_trail[0]), "Hello") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_trail[1]), "World") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_trail[2]), "") == 0);
    raxel_string_destroy(&s_trail);
    for (size_t i = 0; i < raxel_array_size(parts_trail); i++) {
        raxel_string_destroy(&parts_trail[i]);
    }
    raxel_array_destroy(parts_trail);
    
    // Edge case 5: consecutive delimiters.
    raxel_string_t s_consec = raxel_string_create(&allocator, 0);
    raxel_string_append(&s_consec, "Hello,,World");
    raxel_array(raxel_string_t) parts_consec = raxel_string_split(&s_consec, ',');
    RAXEL_TEST_ASSERT(raxel_array_size(parts_consec) == 3);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_consec[0]), "Hello") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_consec[1]), "") == 0);
    RAXEL_TEST_ASSERT(strcmp(raxel_string_data(&parts_consec[2]), "World") == 0);
    raxel_string_destroy(&s_consec);
    for (size_t i = 0; i < raxel_array_size(parts_consec); i++) {
        raxel_string_destroy(&parts_consec[i]);
    }
    raxel_array_destroy(parts_consec);
}

void register_container_tests() {
    RAXEL_TEST_REGISTER(test_array_creation);
    RAXEL_TEST_REGISTER(test_array_iterator);
    RAXEL_TEST_REGISTER(test_array_random_access);
    RAXEL_TEST_REGISTER(test_array_char);
    RAXEL_TEST_REGISTER(test_list_creation_push_back);
    RAXEL_TEST_REGISTER(test_list_resize);
    RAXEL_TEST_REGISTER(test_list_many_push_back);
    RAXEL_TEST_REGISTER(test_list_iterator);
    RAXEL_TEST_REGISTER(test_string_basics);
    RAXEL_TEST_REGISTER(test_string_split);
    RAXEL_TEST_REGISTER(test_string_empty_and_clear);
    RAXEL_TEST_REGISTER(test_string_multiple_appends);
    RAXEL_TEST_REGISTER(test_string_to_cstr);
    RAXEL_TEST_REGISTER(test_string_split_edge_cases);
}
