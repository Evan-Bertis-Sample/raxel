#include "raxel_input.h"

#include <raxel/core/graphics.h>
#include <raxel/core/util.h>

static void __raxel_surface_callback_handler(raxel_surface_t *surface, raxel_key_event_t event) {
    RAXEL_CORE_LOG("Raxel surface callback handler!\n");
    raxel_input_manager_t *manager = (raxel_input_manager_t *)surface->context.input_manager;
    raxel_size_t num_callbacks = raxel_list_size(manager->key_callbacks);
    for (size_t i = 0; i < num_callbacks; i++) {
        raxel_key_callback_t callback = manager->key_callbacks[i];
        if (callback.key == event.key) {
            RAXEL_CORE_LOG("Calling key callback for key %d\n", event.key);
            if (callback.on_button) {
                callback.on_button(event);
            }
        }
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
    manager->__num_keys_up_this_frame = 0;

    if (surface->callbacks.on_key) {
        RAXEL_CORE_LOG_ERROR("raxel_input_manager_create: surface already has a key callback, overwritting...\n");
    }

    surface->callbacks.on_key = __raxel_surface_callback_handler;
    surface->context.input_manager = manager;

    return manager;
}

void raxel_input_manager_update(raxel_input_manager_t *manager) {
    raxel_surface_t *surface = manager->__surface;
    raxel_size_t num_keys_up = manager->__num_keys_up_this_frame;
    for (raxel_size_t i = 0; i < num_keys_up; i++) {
        raxel_size_t key_idx = manager->__keys_up_this_frame[i];
        manager->__key_state[key_idx] = RAXEL_KEY_STATE_UP;
    }
    manager->__num_keys_up_this_frame = 0;
}

void raxel_input_manager_destroy(raxel_input_manager_t *manager) {
    raxel_list_destroy(manager->key_callbacks);
    raxel_list_destroy(manager->mouse_callbacks);
    raxel_free(manager->__allocator, manager);
}

void raxel_input_manager_add_button_callback(raxel_input_manager_t *manager, raxel_key_callback_t callback) {
    raxel_list_push_back(manager->key_callbacks, callback);
}

void raxel_input_manager_add_mouse_callback(raxel_input_manager_t *manager, raxel_mouse_callback_t callback) {
    raxel_list_push_back(manager->mouse_callbacks, callback);
}

int raxel_input_manager_is_key_down(raxel_input_manager_t *manager, raxel_keys_t key) {
    if (manager->__key_state[key] == RAXEL_KEY_STATE_DOWN_THIS_FRAME || manager->__key_state[key] == RAXEL_KEY_STATE_DOWN) {
        return 1;
    }
    return 0;
}

int raxel_input_manager_is_key_up(raxel_input_manager_t *manager, raxel_keys_t key) {
    if (manager->__key_state[key] == RAXEL_KEY_STATE_UP_THIS_FRAME || manager->__key_state[key] == RAXEL_KEY_STATE_UP) {
        return 1;
    }
    return 0;
}

int raxel_input_manager_is_key_pressed(raxel_input_manager_t *manager, raxel_keys_t key) {
    if (manager->__key_state[key] == RAXEL_KEY_STATE_DOWN_THIS_FRAME) {
        return 1;
    }
    return 0;
}