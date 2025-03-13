#include "raxel_surface.h"

#include <raxel/core/util.h>
#include <stdlib.h>
#include <string.h>

static void __key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    RAXEL_CORE_LOG("Key event: key=%d, scancode=%d, action=%d, mods=%d\n", key, scancode, action, mods);
    raxel_surface_t *surface = (raxel_surface_t *)glfwGetWindowUserPointer(window);
    raxel_key_event_t event = {
        .key = key,
        .scancode = scancode,
        .action = action,
        .mods = mods,
    };
    if (surface->callbacks.on_key) {
        RAXEL_CORE_LOG("Key event: key=%d, scancode=%d, action=%d, mods=%d\n", key, scancode, action, mods);
        surface->callbacks.on_key(surface, event);
    }
}

// Internal helper to create a GLFW window.
// Static functions use the __ prefix.
static GLFWwindow *__create_glfw_window(raxel_surface_t *surface, int width, int height, const char *title) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    RAXEL_CORE_LOG("Creating GLFW window\n");
    GLFWwindow *window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (!window) {
        RAXEL_CORE_FATAL_ERROR("Failed to create GLFW window\n");
        exit(EXIT_FAILURE);
    }

    // Set the key callback.
    RAXEL_CORE_LOG("Setting key callback\n");
    glfwSetKeyCallback(window, __key_callback);
    // Set the user pointer to the surface.
    RAXEL_CORE_LOG("Setting window user pointer\n");
    glfwSetWindowUserPointer(window, surface);
    return window;
}

// Internal helper to create a Vulkan surface from a GLFW window.
static VkSurfaceKHR __create_vk_surface(GLFWwindow *window, VkInstance instance) {
    VkSurfaceKHR vk_surface = VK_NULL_HANDLE;
    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &vk_surface));
    return vk_surface;
}

raxel_surface_t *raxel_surface_create(raxel_allocator_t *allocator, const char *title, int width, int height) {
    RAXEL_CORE_LOG("Creating surface\n");
    raxel_surface_t *surface = raxel_malloc(allocator, sizeof(raxel_surface_t));
    if (!surface) {
        RAXEL_CORE_FATAL_ERROR("Failed to allocate memory for raxel_surface_t\n");
        exit(EXIT_FAILURE);
    }

    RAXEL_CORE_LOG("Creating GLFW window\n");
    surface->context.window = __create_glfw_window(surface, width, height, title);
    surface->context.vk_surface = VK_NULL_HANDLE;
    surface->width = width;
    surface->height = height;

    // Create the surface title using our raxel_string API.
    RAXEL_CORE_LOG("Creating surface title\n");
    surface->title = raxel_string_create(allocator, strlen(title) + 1);
    raxel_string_append(&surface->title, title);

    surface->callbacks.on_update = NULL;
    surface->callbacks.on_destroy = NULL;
    surface->callbacks.on_key = NULL;
    surface->callbacks.on_resize = NULL;
    surface->allocator = allocator;

    RAXEL_CORE_LOG("Surface created\n");

    return surface;
}

void raxel_surface_initialize(raxel_surface_t *surface, VkInstance instance) {
    // Initialize the Vulkan surface.
    surface->context.vk_surface = __create_vk_surface(surface->context.window, instance);
}

int raxel_surface_update(raxel_surface_t *surface) {
    // Here you might call a user-defined on_update callback.
    // RAXEL_CORE_LOG("Updating surface\n");
    if (surface->callbacks.on_update) {
        surface->callbacks.on_update(surface);
    }

    // Poll events.
    // RAXEL_CORE_LOG("Polling events\n");
    glfwPollEvents();
    return 0;
}

void raxel_surface_destroy(raxel_surface_t *surface) {
    if (!surface) return;

    // If a user-defined destroy callback exists, call it.
    if (surface->callbacks.on_destroy) {
        surface->callbacks.on_destroy(surface);
    }

    if (surface->context.window) {
        glfwDestroyWindow(surface->context.window);
    }
    raxel_string_destroy(&surface->title);

    raxel_free(surface->allocator, surface);
}
