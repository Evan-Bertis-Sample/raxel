#ifndef __RAXEL_SURFACE_H__
#define __RAXEL_SURFACE_H__

#include <raxel/core/config.h>
#include <raxel/core/util.h>
#include <raxel/core/input.h>

/**------------------------------------------------------------------------
 *                           General Surface Interfaces
 *------------------------------------------------------------------------**/

typedef int raxel_surface_size_t;
typedef struct raxel_surface raxel_surface_t;

typedef struct raxel_surface_callbacks {
    void (*on_update)(raxel_surface_t *surface);
    void (*on_destroy)(raxel_surface_t *surface);

    void (*on_key)(raxel_surface_t *surface, raxel_key_event_t event);
    void (*on_resize)(raxel_surface_t *surface, int width, int height);
} raxel_surface_callbacks_t;


typedef struct raxel_surface {
    void *surface_context;
    raxel_string_t title;
    raxel_surface_size_t width;
    raxel_surface_size_t height;
    raxel_surface_callbacks_t callbacks;
} raxel_surface_t;



// Creating -- macro to choose between which surface to create
#ifdef RAXEL_SURFACE_USE_GLFW
#define raxel_surface_create raxel_surface_create_glfw
#else
#define raxel_surface_create raxel_surface_create_dummy
#endif

void raxel_surface_destroy(raxel_surface_t *surface);
int raxel_surface_update(raxel_surface_t *surface);

raxel_surface_t *raxel_surface_create_dummy(raxel_allocator_t *allocator, raxel_string_t title, raxel_surface_size_t width, raxel_surface_size_t height) {
    RAXEL_CORE_FATAL_ERROR("Dummy surface creation not implemented yet");
    return NULL;
}

/**------------------------------------------------------------------------
 *                           GLFW Surface Implementation
 *------------------------------------------------------------------------**/

#include <glfw/glfw3.h>

typedef struct glfw_surface_context {
    GLFWwindow *window;
} glfw_surface_context_t;

raxel_surface_t *raxel_surface_create_glfw(raxel_allocator_t *allocator, raxel_string_t title, raxel_surface_size_t width, raxel_surface_size_t height);

#endif  // __RAXEL_SURFACE_H__