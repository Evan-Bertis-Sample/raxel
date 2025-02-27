#ifndef __RAXEL_SURFACE_H__
#define __RAXEL_SURFACE_H__

#include <GLFW/glfw3.h>
#include <raxel/core/graphics/vk.h>
#include <raxel/core/util.h>
#include <raxel/core/input.h>

// Use snake_case for everything.
typedef int raxel_surface_size_t;
typedef struct raxel_surface raxel_surface_t;

typedef struct raxel_surface_callbacks {
    void (*on_update)(raxel_surface_t *surface);
    void (*on_destroy)(raxel_surface_t *surface);

    void (*on_key)(raxel_surface_t *surface, raxel_key_event_t event);
    void (*on_resize)(raxel_surface_t *surface, int width, int height);
} raxel_surface_callbacks_t;

typedef struct raxel_surface_context {
    GLFWwindow *window;
    VkSurfaceKHR vk_surface;  // Vulkan surface is now a part of the context.
    // You can add additional Vulkan-specific members if needed.
} raxel_surface_context_t;

struct raxel_surface {
    raxel_surface_context_t *context;
    raxel_string_t title;
    raxel_surface_size_t width;
    raxel_surface_size_t height;
    raxel_surface_callbacks_t callbacks;
    raxel_allocator_t allocator;
};

// Create a surface.
// This function creates a GLFW window (with no client API) and then uses it to create a Vulkan surface.
// 'instance' must be a valid VkInstance.
raxel_surface_t raxel_surface_create(const char *title, int width, int height, VkInstance instance);

// Update the surface (poll events and invoke callbacks, if set).
// Returns 0 on success.
int raxel_surface_update(raxel_surface_t *surface);

// Destroy the surface (destroy the window and free resources).
void raxel_surface_destroy(raxel_surface_t *surface);

#endif  // __RAXEL_SURFACE_H__
