#include "raxel_container.h"

#include <string.h>

#include "raxel_debug.h"
#include "raxel_mem.h"

/**------------------------------------------------------------------------
 *                           RAXEL ARRAY
 *------------------------------------------------------------------------**/

void *__raxel_array_create(raxel_allocator_t *allocator, raxel_size_t size, raxel_size_t stride) {
    raxel_size_t header_size = sizeof(__raxel_array_header_t);
    raxel_size_t data_size = size * stride;
    void *block = raxel_malloc(allocator, header_size + data_size);
    __raxel_array_header_t *header = (__raxel_array_header_t *)block;
    header->__size = size;
    header->__stride = stride;
    header->__allocator = allocator;
    // Return pointer to data (right after header)
    return (void *)((char *)block + header_size);
}

void __raxel_array_destroy(void *array) {
    if (!array) return;
    __raxel_array_header_t *header = raxel_array_header(array);
    raxel_free(header->__allocator, (void *)((char *)array - sizeof(__raxel_array_header_t)));
}

static void *__raxel_array_it_next(raxel_iterator_t *it) {
    __raxel_array_header_t *header = it->__ctx;
    // moove the __data pointer by the stride
    it->__data = (void *)((char *)it->__data + header->__stride);
    return it->__data;
}

static void *__raxel_array_it_current(raxel_iterator_t *it) {
    return it->__data;
}

raxel_iterator_t raxel_array_iterator(void *array) {
    return (raxel_iterator_t){
        .__ctx = raxel_array_header(array),
        .__data = array,
        .next = __raxel_array_it_next,
        .current = __raxel_array_it_current};
}

/**------------------------------------------------------------------------
 *                           RAXEL LIST (continuous memory)
 *------------------------------------------------------------------------**/

void *__raxel_list_create(raxel_allocator_t *allocator, raxel_size_t capacity, raxel_size_t size, raxel_size_t stride) {
    // RAXEL_CORE_LOG("Creating list of size %u, stride %u\n", size, stride);
    raxel_size_t header_size = sizeof(__raxel_list_header_t);
    raxel_size_t data_size = capacity * stride;
    // RAXEL_CORE_LOG("Allocating %u bytes for list (size %u, stride %u)\n", header_size + data_size, size, stride);
    void *block = raxel_malloc(allocator, header_size + data_size);
    __raxel_list_header_t *header = (__raxel_list_header_t *)block;
    header->__size = size;
    header->__stride = stride;
    header->__capacity = capacity;
    header->__allocator = allocator;
    // Return pointer to data (right after header)
    return (void *)((char *)block + header_size);
}

void __raxel_list_destroy(void *list) {
    if (!list) return;
    __raxel_list_header_t *header = raxel_list_header(list);
    raxel_free(header->__allocator, (void *)((char *)list - sizeof(__raxel_list_header_t)));
}

void __raxel_list_resize(void **list_ptr, raxel_size_t new_capacity) {
    if (!list_ptr || !(*list_ptr)) return;

    __raxel_list_header_t *old_header = raxel_list_header(*list_ptr);
    void *old_list = *list_ptr;
    raxel_size_t copy_size = (old_header->__size < new_capacity) ? old_header->__size : new_capacity;
    // RAXEL_CORE_LOG("Resizing list from %u to %u\n", old_header->__capacity, new_capacity);
    void *new_list = __raxel_list_create(old_header->__allocator, new_capacity, copy_size, old_header->__stride);
    __raxel_list_header_t *new_header = raxel_list_header(new_list);

    // Copy old data to new list
    new_header->__size = copy_size;
    new_header->__stride = old_header->__stride;
    new_header->__capacity = new_capacity;

    memcpy(new_list, old_list, copy_size * old_header->__stride);
    __raxel_list_destroy(old_list);

    // Update list pointer
    *list_ptr = new_list;
}

void __raxel_list_push_back(void **list_ptr, void *data) {
    if (!list_ptr || !(*list_ptr)) {
        RAXEL_CORE_LOG("List is NULL\n");
        return;
    }

    // RAXEL_CORE_LOG("Pushing back data onto array of size %u, capacity %u, stride %u\n", raxel_list_size(*list_ptr), raxel_list_capacity(*list_ptr), raxel_list_stride(*list_ptr));
    __raxel_list_header_t *header = raxel_list_header(*list_ptr);
    if (header->__size == header->__capacity) {
        // RAXEL_CORE_LOG("Resizing list to new capacity %u\n", header->__capacity * 2);
        __raxel_list_resize(list_ptr, header->__capacity * 2);
        header = raxel_list_header(*list_ptr);
        // RAXEL_CORE_LOG("New list capacity is %u\n", header->__capacity);
    }
    // Copy data to the end of the list
    raxel_size_t offset = header->__size * header->__stride;
    // RAXEL_CORE_LOG("Copying data of size %u at offset %u\n", header->__stride, offset);
    memcpy((char *)*list_ptr + offset, data, header->__stride);

    header->__size++;

    // RAXEL_CORE_LOG("Push finished, new size is %u, new capacity is %u, stride is %u\n", header->__size, header->__capacity, header->__stride);
}

static void *__raxel_list_it_next(raxel_iterator_t *it) {
    __raxel_list_header_t *header = it->__ctx;
    // moove the __data pointer by the stride
    it->__data = (void *)((char *)it->__data + header->__stride);
    return it->__data;
}

static void *__raxel_list_it_current(raxel_iterator_t *it) {
    return it->__data;
}

raxel_iterator_t raxel_list_iterator(void *list) {
    return (raxel_iterator_t){
        .__ctx = raxel_list_header(list),
        .__data = list,
        .next = __raxel_list_it_next,
        .current = __raxel_list_it_current};
}

/**------------------------------------------------------------------------
 *                           RAXEL STRINGS
 *------------------------------------------------------------------------**/

// Ensure a minimum capacity of 1 so that __data is always non-NULL.
static void __raxel_string_reserve(raxel_string_t *string, raxel_size_t new_capacity) {
    if (new_capacity <= string->__capacity) {
        return;
    }
    char *new_data = (char *)raxel_malloc(string->__allocator, new_capacity + 1);
    if (string->__data && string->__size > 0) {
        memcpy(new_data, string->__data, string->__size);
    }
    new_data[string->__size] = '\0';
    if (string->__data) {
        raxel_free(string->__allocator, string->__data);
    }
    string->__data = new_data;
    string->__capacity = new_capacity;
}

raxel_string_t raxel_string_create(raxel_allocator_t *allocator, raxel_size_t capacity) {
    raxel_string_t string;
    string.__allocator = allocator;
    string.__size = 0;
    string.__capacity = 0;
    string.__data = NULL;
    // Force a minimum capacity of 1.

    if (capacity == 0) {
        capacity = 1;
    }
    __raxel_string_reserve(&string, capacity);
    string.__data[0] = '\0';
    return string;
}

void raxel_string_destroy(raxel_string_t *string) {
    if (!string) return;
    if (string->__data) {
        raxel_free(string->__allocator, string->__data);
    }
    string->__data = NULL;
    string->__size = 0;
    string->__capacity = 0;
    string->__allocator = NULL;
}

void raxel_string_resize(raxel_string_t *string, raxel_size_t size) {
    if (!string) return;
    if (size > string->__capacity) {
        __raxel_string_reserve(string, size);
    }
    string->__size = size;
    string->__data[size] = '\0';
}

void raxel_string_push_back(raxel_string_t *string, char c) {
    if (!string) return;
    if (string->__size == string->__capacity) {
        raxel_size_t new_cap = (string->__capacity == 0) ? 8 : (string->__capacity * 2);
        __raxel_string_reserve(string, new_cap);
    }
    string->__data[string->__size++] = c;
    string->__data[string->__size] = '\0';
}

void raxel_string_append_n(raxel_string_t *string, const char *str, raxel_size_t n) {
    if (!string || !str || n == 0) return;
    if (string->__size + n > string->__capacity) {
        raxel_size_t new_cap = string->__capacity;
        while (string->__size + n > new_cap) {
            new_cap = (new_cap == 0) ? 8 : (new_cap * 2);
        }
        __raxel_string_reserve(string, new_cap);
    }
    memcpy(string->__data + string->__size, str, n);
    string->__size += n;
    string->__data[string->__size] = '\0';
}

void raxel_string_append(raxel_string_t *string, const char *str) {
    if (!string || !str) return;
    raxel_size_t len = strlen(str);
    raxel_string_append_n(string, str, len);
}

char *raxel_string_data(raxel_string_t *string) {
    if (!string) return NULL;
    return string->__data;
}

char *raxel_string_to_cstr(raxel_string_t *string) {
    if (!string) return NULL;
    string->__data[string->__size] = '\0';
    return string->__data;
}

void raxel_string_clear(raxel_string_t *string) {
    if (!string) return;
    string->__size = 0;
    if (string->__data) {
        string->__data[0] = '\0';
    }
}

/**
 * Splits the given string by the specified delimiter and returns
 * a raxel_array(raxel_string_t) with each token.
 *
 * Each raxel_string_t in the returned array is independently allocated.
 * The caller must call raxel_string_destroy() on each token and then
 * raxel_array_destroy(...) on the array when done.
 */
raxel_array(raxel_string_t) raxel_string_split(raxel_string_t *string, char delim) {
    // If string pointer is NULL, return an empty array.
    if (!string) {
        return raxel_array_create(raxel_string_t, NULL, 0);
    }
    // If the string is empty, return an array with one empty token.
    if (string->__size == 0) {
        raxel_array(raxel_string_t) result = raxel_array_create(raxel_string_t, string->__allocator, 1);
        raxel_string_t empty_str = raxel_string_create(string->__allocator, 1);
        empty_str.__data[0] = '\0';
        result[0] = empty_str;
        return result;
    }
    // First pass: count delimiters.
    raxel_size_t delim_count = 0;
    for (raxel_size_t i = 0; i < string->__size; i++) {
        if (string->__data[i] == delim) {
            delim_count++;
        }
    }
    raxel_size_t substring_count = delim_count + 1;
    raxel_array(raxel_string_t) result = raxel_array_create(raxel_string_t, string->__allocator, substring_count);

    // Second pass: split into substrings.
    raxel_size_t start_idx = 0;
    raxel_size_t result_idx = 0;
    for (raxel_size_t i = 0; i < string->__size; i++) {
        if (string->__data[i] == delim) {
            raxel_size_t length = i - start_idx;
            // Ensure non-zero capacity even for an empty token.
            raxel_string_t s = raxel_string_create(string->__allocator, (length > 0 ? length : 1));
            if (length > 0) {
                raxel_string_append_n(&s, &string->__data[start_idx], length);
            }
            result[result_idx++] = s;
            start_idx = i + 1;
        }
    }
    // Final substring (after last delimiter).
    {
        raxel_size_t length = string->__size - start_idx;
        raxel_string_t s = raxel_string_create(string->__allocator, (length > 0 ? length : 1));
        if (length > 0) {
            raxel_string_append_n(&s, &string->__data[start_idx], length);
        }
        result[result_idx++] = s;
    }
    return result;
}