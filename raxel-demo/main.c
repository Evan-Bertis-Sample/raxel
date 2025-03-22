#include <cglm/cglm.h>
#include <math.h>
#include <raxel/core/graphics.h>
#include <raxel/core/graphics/passes/clear_color_pass.h>
#include <raxel/core/graphics/passes/compute_pass.h>
#include <raxel/core/input.h>
#include <raxel/core/util.h>
#include <raxel/core/voxel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WIDTH 800
#define HEIGHT 600

// Called when the surface is destroyed.
static void on_destroy(raxel_surface_t *surface) {
    RAXEL_CORE_LOG("Destroying surface...\n");
    raxel_pipeline_t *pipeline = (raxel_pipeline_t *)surface->user_data;
    raxel_pipeline_cleanup(pipeline);
    raxel_pipeline_destroy(pipeline);
}

int main(void) {
    // Initialize random seed.
    srand((unsigned)time(NULL));

    // Create a default allocator.
    raxel_allocator_t allocator = raxel_default_allocator();

    // Create a window and Vulkan surface.
    raxel_surface_t *surface = raxel_surface_create(&allocator, "Voxel Raymarch", WIDTH, HEIGHT);

    // Setup input.
    raxel_input_manager_t *input_manager = raxel_input_manager_create(&allocator, surface);

    // Create the pipeline.
    raxel_pipeline_t *pipeline = raxel_pipeline_create(&allocator, surface);
    surface->user_data = pipeline;

    // Initialize the pipeline (creates instance, device, swapchain, etc.).
    raxel_pipeline_initialize(pipeline);

    // Set the debug target to the internal color target.
    raxel_pipeline_set_debug_target(pipeline, RAXEL_PIPELINE_TARGET_COLOR);

    // Create a clear pass to clear the internal color target.
    raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.3f, 0.8f, 1.0f});
    raxel_pipeline_add_pass(pipeline, clear_pass);


    // Create the compute shader and pass.
    raxel_pc_buffer_desc_t pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){.name = "view", .offset = 0, .size = 16 * sizeof(float)},
        (raxel_pc_entry_t){.name = "fov", .offset = 16 * sizeof(float), .size = sizeof(float)},
        (raxel_pc_entry_t){.name = "rays_per_pixel", .offset = 16 * sizeof(float) + sizeof(float), .size = sizeof(int)},
        (raxel_pc_entry_t){.name = "debug_mode", .offset = 16 * sizeof(float) + sizeof(float) + sizeof(int), .size = sizeof(int)}, );
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/voxel.comp.spv", &pc_desc);

    // Create a compute pass context.
    raxel_compute_pass_context_t *compute_ctx = raxel_malloc(&allocator, sizeof(raxel_compute_pass_context_t));
    compute_ctx->compute_shader = compute_shader;
    // Set dispatch dimensions based on our window size and workgroup size.
    compute_ctx->dispatch_x = (WIDTH + 15) / 16;
    compute_ctx->dispatch_y = (HEIGHT + 15) / 16;
    compute_ctx->dispatch_z = 1;
    // Set the blit target to the internal color target.
    compute_ctx->targets[0] = RAXEL_PIPELINE_TARGET_COLOR;
    compute_ctx->targets[1] = -1;  // Sentinel.
    compute_ctx->on_dispatch_finished = NULL;

    // Create the compute pass and add it to the pipeline.
    raxel_pipeline_pass_t compute_pass = raxel_compute_pass_create(compute_ctx);
    raxel_pipeline_add_pass(pipeline, compute_pass);

    // --- Create and populate a voxel world ---
    raxel_voxel_world_t *world = raxel_voxel_world_create(&allocator);

    // Create a giant sphere at the origin
    int radius = 50;

    for (int x = -radius; x <= radius; x++) {
        for (int y = -radius; y <= radius; y++) {
            for (int z = -radius; z <= radius; z++) {
                if (x * x + y * y + z * z <= radius * radius) {

                    raxel_voxel_t sphere_voxel = {
                        .material = 255,
                    };

                    raxel_voxel_world_place_voxel(world, x, y, z, sphere_voxel);
                }
            }
        }
    }

    vec3 camera_position = {0.0f, 0.0f, -50.0f};
    float camera_rotation = 0.0f;

    raxel_voxel_world_set_sb(world, compute_shader, pipeline);
    raxel_voxel_world_update_options_t initial_options = {0};
    initial_options.camera_position[0] = camera_position[0];
    initial_options.camera_position[1] = camera_position[1];
    initial_options.camera_position[2] = camera_position[2];
    initial_options.view_distance = 100.0f;
    raxel_voxel_world_update(world, &initial_options);
    raxel_voxel_world_dispatch_sb(world, compute_shader, pipeline);

    raxel_pipeline_start(pipeline);

    // --- Main Loop ---
    // For demonstration, we update the push constants per frame.
    double time = 0.0;
    double delta_time = 0.01;


    while (!raxel_pipeline_should_close(pipeline)) {
        time += delta_time;;

        // Simple WASD and QE controls to move the camera.
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_W)) {
            camera_position[2] += 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_S)) {
            camera_position[2] -= 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_A)) {
            camera_position[0] -= 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_D)) {
            camera_position[0] += 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_SPACE)) {
            camera_position[1] += 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_LEFT_SHIFT)) {
            camera_position[1] -= 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_Q)) {
            camera_rotation -= 0.1f;
        }
        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_E)) {
            camera_rotation += 0.1f;
        }

        if (raxel_input_manager_is_key_down(input_manager, RAXEL_KEY_ESCAPE)) {
            break;
        }

        // use the number keys to switch debug modes 1 is normal, 2 is raymarch debug, 3 is data debug
        if (raxel_input_manager_is_key_pressed(input_manager, RAXEL_KEY_1)) {
            int debug_mode = 0;
            RAXEL_APP_LOG("Setting debug mode to 0\n");
            raxel_pc_buffer_set(compute_shader->pc_buffer, "debug_mode", &debug_mode);
        }
        if (raxel_input_manager_is_key_pressed(input_manager, RAXEL_KEY_2)) {
            int debug_mode = 1;
            RAXEL_APP_LOG("Setting debug mode to 1\n");
            raxel_pc_buffer_set(compute_shader->pc_buffer, "debug_mode", &debug_mode);
        }
        if (raxel_input_manager_is_key_pressed(input_manager, RAXEL_KEY_3)) {
            int debug_mode = 2;
            RAXEL_APP_LOG("Setting debug mode to 2\n");
            raxel_pc_buffer_set(compute_shader->pc_buffer, "debug_mode", &debug_mode);
        }

        // RAXEL_APP_LOG("Camera position: (%f, %f, %f)\n", camera_position[0], camera_position[1], camera_position[2]);

        // Update the view matrix.
        mat4 view;
        glm_mat4_identity(view);

        // Rotate the view matrix by the camera rotation.
        glm_rotate(view, camera_rotation, (vec3){0.0f, 1.0f, 0.0f});

        // Translate the view matrix by the negative camera position.
        glm_translate(view, (vec3){-camera_position[0], -camera_position[1], -camera_position[2]});

        raxel_pc_buffer_set(compute_shader->pc_buffer, "view", view);

        // Update fov (e.g., 90 degrees converted to radians).
        float fov = glm_rad(90.0f);
        raxel_pc_buffer_set(compute_shader->pc_buffer, "fov", &fov);

        // Update rays per pixel (e.g., 4 rays per pixel).
        int rpp = 1;
        raxel_pc_buffer_set(compute_shader->pc_buffer, "rays_per_pixel", &rpp);


        // Update the storage buffer with the new voxel world data.
        raxel_voxel_world_update_options_t options = {0};
        options.camera_position[0] = camera_position[0];
        options.camera_position[1] = camera_position[1];
        options.camera_position[2] = camera_position[2];
        options.view_distance = 100.0f;

        RAXEL_CORE_LOG("Camera position: (%f, %f, %f)\n", camera_position[0], camera_position[1], camera_position[2]);

        // raxel_voxel_world_update(world, &options);
        // raxel_voxel_world_dispatch_sb(world, compute_shader, pipeline)

        raxel_pipeline_update(pipeline);
    }

    return 0;
}
