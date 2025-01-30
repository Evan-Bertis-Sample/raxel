#ifndef __RAXEL_SURFACE_H__
#define __RAXEL_SURFACE_H__

#include <raxel/core/util.h>

typedef int raxel_surface_size_t;

typedef struct raxel_surface_callbacks {
    void (*on_create)(void *context);
    void (*on_update)(void *context);
    void (*on_destroy)(void *context);

    void (*on_key)(void *context, int key, int scancode, int action, int mods);
    void (*on_resize)(void *context, int width, int height);
} raxel_surface_callbacks_t;

typedef struct raxel_surface {
    void *surface_context;
    raxel_string_t title;
    raxel_surface_size_t width;
    raxel_surface_size_t height;
    raxel_surface_callbacks_t callbacks;
} raxel_surface_t;

void raxel_surface_create(raxel_surface_t *surface, raxel_string_t title, raxel_surface_size_t width, raxel_surface_size_t height);


#endif // __RAXEL_SURFACE_H__