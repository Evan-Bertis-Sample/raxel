#include "raxel_surface.h"

#include <raxel/core/util.h>
#include <stdlib.h>
#include <string.h>

// Internal helper to create a GLFW window.
// Static functions use the __ prefix.
static GLFWwindow *__create_glfw_window(int width, int height, const char *title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return glfwCreateWindow(width, height, title, NULL, NULL);
}

// Internal helper to create a Vulkan surface from a GLFW window.
static VkSurfaceKHR __create_vk_surface(GLFWwindow *window, VkInstance instance) {
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &vk_surface));
    return vk_surface;
}

raxel_surface_t raxel_surface_create(const char *title, int width, int height) {
    raxel_surface_t surface = {0};

    // Allocate and initialize the context.
    surface.context = malloc(sizeof(raxel_surface_context_t));
    if (!surface.context) {
        fprintf(stderr, "Failed to allocate raxel_surface_context_t\n");
        exit(EXIT_FAILURE);
    }
    surface.context->window = __create_glfw_window(width, height, title);
    surface.context->vk_surface = VK_NULL_HANDLE;
    surface.width = width;
    surface.height = height;
    surface.allocator = raxel_default_allocator();

    // Create the surface title using our raxel_string API.
    surface.title = raxel_string_create(&surface.allocator, strlen(title) + 1);
    raxel_string_append(&surface.title, title);

    // Initialize callbacks to NULL.
    memset(&surface.callbacks, 0, sizeof(surface.callbacks));

    return surface;
}

void raxel_surface_initialize(raxel_surface_t *surface, VkInstance instance) {
    // Initialize the Vulkan surface.
    surface->context->vk_surface = __create_vk_surface(surface->context->window, instance);
}

int raxel_surface_update(raxel_surface_t *surface) {
    // Here you might call a user-defined on_update callback.
    if (surface->callbacks.on_update) {
        surface->callbacks.on_update(surface);
    }
    // For now, simply return 0 to indicate success.
    return 0;
}

void raxel_surface_destroy(raxel_surface_t *surface) {
    if (!surface) return;

    // If a user-defined destroy callback exists, call it.
    if (surface->callbacks.on_destroy) {
        surface->callbacks.on_destroy(surface);
    }
    if (surface->context) {
        if (surface->context->window) {
            glfwDestroyWindow(surface->context->window);
        }
        // Typically, you would destroy the Vulkan surface here.
        // Since vkDestroySurfaceKHR requires the VkInstance, ensure that is handled appropriately.
        // For example:
        // vkDestroySurfaceKHR(instance, surface->context->vk_surface, NULL);
        free(surface->context);
        surface->context = NULL;
    }
    raxel_string_destroy(&surface->title);
}
