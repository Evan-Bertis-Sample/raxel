#include "raxel_container.h"

#include <string.h>

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
    __raxel_array_header_t *header = raxel_array_header(it->__data);
    return (void *)((char *)it->__data + header->__stride);
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
 *                           RAXEL LIST (continuous memory)
 *------------------------------------------------------------------------**/

void *__raxel_list_create(raxel_allocator_t *allocator, raxel_size_t size, raxel_size_t stride) {
    raxel_size_t header_size = sizeof(__raxel_list_header_t);
    raxel_size_t data_size = size * stride;
    void *block = raxel_malloc(allocator, header_size + data_size);
    __raxel_list_header_t *header = (__raxel_list_header_t *)block;
    header->__size = size;
    header->__stride = stride;
    header->__capacity = size;
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
    raxel_size_t header_size = sizeof(__raxel_list_header_t);
    raxel_size_t data_size = new_capacity * old_header->__stride;
    void *new_block = raxel_malloc(old_header->__allocator, header_size + data_size);
    __raxel_list_header_t *new_header = (__raxel_list_header_t *)new_block;
    /* Copy header information from old block */
    memcpy(new_header, old_header, header_size);
    /* Copy data from old block into new block */
    memcpy((char *)new_block + header_size, *list_ptr, old_header->__size * old_header->__stride);
    new_header->__capacity = new_capacity;  // update capacity field
    raxel_free(old_header->__allocator, (void *)((char *)*list_ptr - header_size));
    *list_ptr = (void *)((char *)new_block + header_size);
}

void __raxel_list_push_back(void **list_ptr, void *data) {
    if (!list_ptr || !(*list_ptr)) return;
    __raxel_list_header_t *header = raxel_list_header(*list_ptr);
    if (header->__size == header->__capacity) {
        __raxel_list_resize(list_ptr, header->__capacity * 2);
        header = raxel_list_header(*list_ptr);
    }
    memcpy((char *)*list_ptr + header->__size * header->__stride, data, header->__stride);
    header->__size++;
}

static void *__raxel_list_it_next(raxel_iterator_t *it) {
    __raxel_list_header_t *header = raxel_list_header(it->__data);
    return (void *)((char *)it->__data + header->__stride);
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
    // Allocate new buffer (plus one for the null terminator)
    char *new_data = (char *)raxel_malloc(string->__allocator, new_capacity + 1);
    // Copy old data if any
    if (string->__data && string->__size > 0) {
        memcpy(new_data, string->__data, string->__size);
    }
    // Null-terminate
    new_data[string->__size] = '\0';
    // Free old buffer if allocated
    if (string->__data) {
        raxel_free(string->__allocator, string->__data);
    }
    // Update string struct
    string->__data = new_data;
    string->__capacity = new_capacity;
}

raxel_string_t raxel_string_create(raxel_allocator_t *allocator, raxel_size_t capacity) {
    raxel_string_t string;
    string.__allocator = allocator;
    string.__size = 0;
    string.__capacity = 0;
    string.__data = NULL;
    // Reserve initial capacity
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

    // First pass: count delimiters to determine number of substrings
    raxel_size_t delim_count = 0;
    for (raxel_size_t i = 0; i < string->__size; i++) {
        if (string->__data[i] == delim) {
            delim_count++;
        }
    }
    raxel_size_t substring_count = delim_count + 1;
    raxel_array(raxel_string_t) result = raxel_array_create(raxel_string_t, string->__allocator, substring_count);

    // Second pass: split into substrings
    raxel_size_t start_idx = 0;
    raxel_size_t result_idx = 0;
    for (raxel_size_t i = 0; i < string->__size; i++) {
        if (string->__data[i] == delim) {
            raxel_size_t length = i - start_idx;
            raxel_string_t s = raxel_string_create(string->__allocator, length);
            raxel_string_append_n(&s, &string->__data[start_idx], length);
            result[result_idx++] = s;
            start_idx = i + 1;
        }
    }
    // Final substring (from last delimiter to end)
    {
        raxel_size_t length = string->__size - start_idx;
        raxel_string_t s = raxel_string_create(string->__allocator, length);
        raxel_string_append_n(&s, &string->__data[start_idx], length);
        result[result_idx++] = s;
    }
    return result;
}
