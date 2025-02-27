#ifndef __VK_H__
#define __VK_H__

#include <raxel/core/util/raxel_debug.h>
#include <vulkan/vulkan.h>

#ifndef VK_CHECK
#define VK_CHECK(x)                                                   \
    do {                                                              \
        VkResult err = x;                                             \
        if (err != VK_SUCCESS) {                                      \
            RAXEL_CORE_LOG_ERROR("Detected Vulkan error: %d\n", err); \
            exit(EXIT_FAILURE);                                       \
        }                                                             \
    } while (0)
#endif

#endif  // __VK_H__