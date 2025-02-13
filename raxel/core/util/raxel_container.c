#include "raxel_container.h"

#include <string.h>

#include "raxel_mem.h"

/**------------------------------------------------------------------------
 *                           RAXEL ARRAY
 *------------------------------------------------------------------------**/

void *__raxel_array_create(raxel_allocator_t *allocator, raxel_size_t size, raxel_size_t stride) {
    raxel_size_t header_size = sizeof(__raxel_array_header_t);
    raxel_size_t data_size = size * stride;
    void *array = raxel_malloc(allocator, header_size + data_size);
    __raxel_array_header_t *header = array;
    *header = (__raxel_array_header_t){
        .__size = size,
        .__stride = stride};
    // return the pointer to the data
    return (void *)((raxel_size_t *)array + header_size);
}

void __raxel_array_destroy(raxel_allocator_t *allocator, void *array) {
    // get the header
    raxel_free(allocator, (void *)((raxel_size_t *)array - sizeof(__raxel_array_header_t)));
}

// iterators are implemented wrong rn

static void *__raxel_array_it_next(raxel_iterator_t *it) {
    __raxel_array_header_t *header = (__raxel_array_header_t *)((raxel_size_t *)it->__data - sizeof(__raxel_array_header_t));
    return (void *)((raxel_size_t *)it->__data + header->__stride);
}

static void *__raxel_array_it_current(raxel_iterator_t *it) {
    return it->__data;
}

raxel_iterator_t raxel_array_iterator(void *array) {
    return (raxel_iterator_t){
        .__data = array,
        .next = __raxel_array_it_next,
        .current = __raxel_array_it_current};
}

/**------------------------------------------------------------------------
 *                           RAXEL_LIST (continuous memory)
 *------------------------------------------------------------------------**/

void *__raxel_list_create(raxel_allocator_t *allocator, raxel_size_t size, raxel_size_t stride) {
    raxel_size_t header_size = sizeof(__raxel_list_header_t);
    raxel_size_t data_size = size * stride;
    void *list = raxel_malloc(allocator, header_size + data_size);
    __raxel_list_header_t *header = list;
    *header = (__raxel_list_header_t){
        .__size = size,
        .__stride = stride,
        .__capacity = size,
        .__allocator = allocator};
    // return the pointer to the data
    return (void *)((raxel_size_t *)list + header_size);
}

void __raxel_list_destroy(void *list) {
    __raxel_list_header_t *header = (__raxel_list_header_t *)((raxel_size_t *)list - sizeof(__raxel_list_header_t));
    raxel_free(header->__allocator, (void *)((raxel_size_t *)list - sizeof(__raxel_list_header_t)));
}

void __raxel_list_resize(void **list_ptr, raxel_size_t size) {
    __raxel_list_header_t *header = (__raxel_list_header_t *)((raxel_size_t *)*list_ptr - sizeof(__raxel_list_header_t));
    raxel_size_t header_size = sizeof(__raxel_list_header_t);
    raxel_size_t data_size = size * header->__stride;
    void *new_list = raxel_malloc(header->__allocator, header_size + data_size);
    memcpy(new_list, *list_ptr, header->__size * header->__stride);
    raxel_free(header->__allocator, (void *)((raxel_size_t *)*list_ptr - sizeof(__raxel_list_header_t)));
    *list_ptr = (void *)((raxel_size_t *)new_list + header_size);
}

void __raxel_list_push_back(void **list_ptr, void *data) {
    __raxel_list_header_t *header = (__raxel_list_header_t *)((raxel_size_t *)*list_ptr - sizeof(__raxel_list_header_t));
    if (header->__size == header->__capacity) {
        __raxel_list_resize(list_ptr, header->__capacity * 2);
        header->__capacity *= 2;
    }
    memcpy((void *)((raxel_size_t *)*list_ptr + header->__size * header->__stride), data, header->__stride);
    header->__size++;
}

// iterators are implemented wrong rn

static void *__raxel_list_it_next(raxel_iterator_t *it) {
    __raxel_list_header_t *header = (__raxel_list_header_t *)((raxel_size_t *)it->__data - sizeof(__raxel_list_header_t));
    return (void *)((raxel_size_t *)it->__data + header->__stride);
}

static void *__raxel_list_it_current(raxel_iterator_t *it) {
    return it->__data;
}

raxel_iterator_t raxel_list_iterator(void *list) {
    return (raxel_iterator_t){
        .__data = list,
        .next = __raxel_list_it_next,
        .current = __raxel_list_it_current};
}

/**------------------------------------------------------------------------
 *                           RAXEL STRINGS
 *------------------------------------------------------------------------**/

static void __raxel_string_reserve(raxel_string_t *string, raxel_size_t new_capacity) {
    if (new_capacity <= string->__capacity) {
        // Already have enough capacity
        return;
    }
    // Allocate new buffer
    char *new_data = (char *)raxel_malloc(string->__allocator, new_capacity + 1);
    // Copy old data
    if (string->__data && string->__size > 0) {
        memcpy(new_data, string->__data, string->__size);
    }
    // Null-terminate
    new_data[string->__size] = '\0';
    // Free old buffer
    if (string->__data) {
        raxel_free(string->__allocator, string->__data);
    }
    // Update string
    string->__data = new_data;
    string->__capacity = new_capacity;
}

raxel_string_t raxel_string_create(raxel_allocator_t *allocator, raxel_size_t capacity) {
    raxel_string_t string;
    string.__allocator = allocator;
    string.__size = 0;
    string.__capacity = 0;
    string.__data = NULL;

    // Reserve initial capacity (plus 1 for the null terminator)
    __raxel_string_reserve(&string, capacity);
    if (string.__data) {
        string.__data[0] = '\0';
    }
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

    // If we need more capacity, reserve
    if (size > string->__capacity) {
        __raxel_string_reserve(string, size);
    }

    // If shrinking, just adjust size
    string->__size = size;
    // Always null-terminate
    string->__data[size] = '\0';
}

void raxel_string_push_back(raxel_string_t *string, char c) {
    if (!string) return;

    // Expand if needed
    if (string->__size == string->__capacity) {
        raxel_size_t new_cap = (string->__capacity == 0) ? 8 : (string->__capacity * 2);
        __raxel_string_reserve(string, new_cap);
    }

    string->__data[string->__size++] = c;
    string->__data[string->__size] = '\0';
}

void raxel_string_append_n(raxel_string_t *string, const char *str, raxel_size_t n) {
    if (!string || !str || n == 0) return;

    // Ensure enough capacity
    if (string->__size + n > string->__capacity) {
        raxel_size_t new_cap = string->__capacity;
        while (string->__size + n > new_cap) {
            new_cap = (new_cap == 0) ? 8 : (new_cap * 2);
        }
        __raxel_string_reserve(string, new_cap);
    }

    memcpy(string->__data + string->__size, str, n);
    string->__size += n;
    // Null-terminate
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
    // Ensure null termination
    if (string->__data && string->__size <= string->__capacity) {
        string->__data[string->__size] = '\0';
    }
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
 * NOTE: Each raxel_string_t in the returned array is *independently* allocated.
 * The caller must call raxel_string_destroy() on each and then
 * raxel_array_destroy(...) on the array when done.
 */
raxel_array(raxel_string_t) raxel_string_split(raxel_string_t *string, char delim) {
    if (!string || !string->__data) {
        // Return an empty array
        return raxel_array_create(raxel_string_t, string ? string->__allocator : NULL, 0);
    }

    // First pass: count how many delimiters (which determines array size)
    raxel_size_t delim_count = 0;
    for (raxel_size_t i = 0; i < string->__size; i++) {
        if (string->__data[i] == delim) {
            delim_count++;
        }
    }
    // We will have delim_count + 1 substrings
    raxel_size_t substring_count = delim_count + 1;

    // Allocate array of raxel_string_t
    raxel_array(raxel_string_t) result =
        raxel_array_create(raxel_string_t, string->__allocator, substring_count);

    // Second pass: split
    raxel_size_t start_idx = 0;
    raxel_size_t result_idx = 0;
    for (raxel_size_t i = 0; i < string->__size; i++) {
        if (string->__data[i] == delim) {
            // Create substring from [start_idx..i)
            raxel_size_t length = i - start_idx;
            raxel_string_t s = raxel_string_create(string->__allocator, length);
            raxel_string_append_n(&s, &string->__data[start_idx], length);

            // Store in array
            result[result_idx++] = s;
            // Move start past delimiter
            start_idx = i + 1;
        }
    }

    // Add the final substring (from start_idx to end)
    {
        raxel_size_t length = string->__size - start_idx;
        raxel_string_t s = raxel_string_create(string->__allocator, length);
        raxel_string_append_n(&s, &string->__data[start_idx], length);
        result[result_idx++] = s;
    }

    return result;
}
