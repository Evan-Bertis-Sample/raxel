#include "raxel_input.h"

#include <raxel/core/graphics.h>
#include <raxel/core/util.h>

static void __raxel_surface_callback_handler(raxel_surface_t *surface, raxel_key_event_t event) {
    raxel_input_manager_t *manager = (raxel_input_manager_t *)surface->callbacks.ctx;
    raxel_size_t num_callbacks = raxel_list_size(manager->key_callbacks);
    for (size_t i = 0; i < num_callbacks; i++) {
        raxel_key_callback_t callback = manager->key_callbacks[i];
        callback(event);
    }
}

raxel_input_manager_t *raxel_input_manager_create(raxel_allocator_t *allocator, raxel_surface_t *surface) {
    if (!surface) {
        RAXEL_CORE_FATAL_ERROR("raxel_input_manager_create: surface is NULL\n");
    }

    raxel_input_manager_t *manager = raxel_malloc(allocator, sizeof(raxel_input_manager_t));
    manager->__surface = surface;
    manager->__allocator = allocator;
    manager->key_callbacks = raxel_list_create_reserve(raxel_key_callback_t, allocator, 0);
    manager->mouse_callbacks = raxel_list_create_reserve(raxel_mouse_callback_t, allocator, 0);

    if (surface->callbacks.on_key) {
        RAXEL_CORE_LOG_ERROR("raxel_input_manager_create: surface already has a key callback, overwritting...\n");
        surface->callbacks.on_key = __raxel_surface_callback_handler;
    }

    return manager;
}

void raxel_input_manager_destroy(raxel_input_manager_t *manager) {
    raxel_list_destroy(manager->key_callbacks);
    raxel_list_destroy(manager->mouse_callbacks);
    raxel_free(manager->__allocator, manager);
}

void raxel_input_manager_add_button_callback(raxel_input_manager_t *manager, (void *callback)(raxel_key_event_t)) {
    raxel_key_callback_t key_callback = {
        .on_button = callback
    };

    raxel_list_push_back(manager->key_callbacks, key_callback);
}

void raxel_input_manager_add_mouse_callback(raxel_input_manager_t *manager, (void *callback)(raxel_mouse_event_t)) {
    raxel_mouse_callback_t mouse_callback = {
        .on_mouse = callback
    };

    raxel_list_push_back(manager->mouse_callbacks, mouse_callback);
}
