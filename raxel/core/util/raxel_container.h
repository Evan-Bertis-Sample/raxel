#ifndef __RAXEL_CONTAINER_H__
#define __RAXEL_CONTAINER_H__

#include <stdint.h>

#include "raxel_mem.h"

typedef size_t raxel_size_t;

/**------------------------------------------------------------------------
 *                           RAXEl ITERATOR
 *------------------------------------------------------------------------**/

typedef struct raxel_iterator {
    void *__data;
    void *(*next)(struct raxel_iterator *it);
    void *(*prev)(struct raxel_iterator *it);
    void *(*current)(struct raxel_iterator *it);
} raxel_iterator_t;

#define RAXEL_ITERATE(type, it, start, end)                                               \
    for (raxel_iterator_t it = raxel_iterator(start, end); it.current(&it); it.next(&it)) \
        for (type *it = (type *)it.current(&it); it; it = (type *)it.next(&it))

/**------------------------------------------------------------------------
 *                           RAXEL ARRAY
 *------------------------------------------------------------------------**/

void *__raxel_array_create(raxel_allocator_t *allocator, raxel_size_t size, raxel_size_t stride);
inline void __raxel_array_destroy(raxel_allocator_t *allocator, void *array);

typedef struct __raxel_array_header {
    raxel_size_t __size;
    raxel_size_t __stride;
} __raxel_array_header_t;

#define raxel_array(__T) __T *

#define raxel_array_create(type, allocator, size) \
    (raxel_array(type))__raxel_array_create(allocator, size, sizeof(type))

#define raxel_array_destroy(allocator, array) \
    __raxel_array_destroy(allocator, (void *)array)

#define raxel_array_size(array) \
    ((__raxel_array_header_t *)((void *)array - sizeof(__raxel_array_header_t)))->__size

#define raxel_array_stride(array) \
    ((__raxel_array_header_t *)((void *)array - sizeof(__raxel_array_header_t)))->__stride


/**------------------------------------------------------------------------
 *                           RAXEL_LIST (continuous memory)
 *------------------------------------------------------------------------**/

void *__raxel_list_create(raxel_allocator_t *allocator, raxel_size_t size, raxel_size_t stride);
inline void __raxel_list_destroy(void *list);
void __raxel_list_resize(void **list, raxel_size_t size);
void __raxel_list_push_back(void **list, void *data);

typedef struct __raxel_list_header {
    raxel_size_t __size;
    raxel_size_t __stride;
    raxel_size_t __capacity;
    raxel_allocator_t *__allocator;
} __raxel_list_header_t;

#define raxel_list(__T) __T *

#define raxel_list_create(type, allocator, size) \
    (raxel_list(type))__raxel_list_create(allocator, size, sizeof(type))

#define raxel_list_destroy(list) \
    __raxel_list_destroy((void *)list)

#define raxel_list_size(list) \
    ((__raxel_list_header_t *)((void *)list - sizeof(__raxel_list_header_t)))->__size

#define raxel_list_capacity(list) \
    ((__raxel_list_header_t *)((void *)list - sizeof(__raxel_list_header_t)))->__capacity

#define raxel_list_allocator(list) \
    ((__raxel_list_header_t *)((void *)list - sizeof(__raxel_list_header_t)))->__allocator

#define raxel_list_resize(list, size) \
    __raxel_list_resize((void **)&list, size)

#define raxel_list_push_back(list, data) \
    __raxel_list_push_back((void **)&list, (void *)data)



#endif  // __RAXEL_CONTAINER_H__