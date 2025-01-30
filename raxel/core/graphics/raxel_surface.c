#include <raxel/core/util.h>
#include "raxel_surface.h"

// #define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

void raxel_window_test()
{

    if (!glfwInit())
    {
        RAXEL_CORE_FATAL_ERROR("Failed to initialize GLFW\n");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(640, 480, "Window Title", NULL, NULL);
    if (!window)
    {
        RAXEL_CORE_FATAL_ERROR("Failed to create window\n");
    }

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

}