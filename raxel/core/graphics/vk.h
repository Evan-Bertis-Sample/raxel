#ifndef __VK_H__
#define __VK_H__

#include <vulkan/vulkan.h>

#ifndef VK_CHECK
#define VK_CHECK(x)                                     \
    do {                                                \
        VkResult err = x;                               \
        if (err != VK_SUCCESS) {                        \
            fprintf(stderr, "Vulkan error: %d\n", err); \
            exit(EXIT_FAILURE);                         \
        }                                               \
    } while (0)
#endif

#endif // __VK_H__