#include <raxel/core/util.h>
#include "raxel_surface.h"

// #define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

raxel_surface_t *raxel_surface_create_glfw(raxel_allocator_t *allocator, raxel_string_t title, raxel_surface_size_t width, raxel_surface_size_t height)
{
    raxel_surface_t *surface = raxel_malloc(allocator, sizeof(raxel_surface_t));
    surface->surface_context = raxel_malloc(allocator, sizeof(glfw_surface_context_t));
    surface->title = title;
    surface->width = width;
    surface->height = height;

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, raxel_cstr(title), NULL, NULL);
    glfw_surface_context_t *context = (glfw_surface_context_t *)surface->surface_context;
    context->window = window;

    return surface;
}
