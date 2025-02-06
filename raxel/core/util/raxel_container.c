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
