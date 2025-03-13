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
#include <time.h>

#define WIDTH 800
#define HEIGHT 600

// Dummy callback for input events.
static void on_key(raxel_key_event_t event) {
    RAXEL_CORE_LOG("Key pressed: %d\n", event.key);
}

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

    // Set up input (dummy).
    raxel_input_manager_t *input_manager = raxel_input_manager_create(&allocator, surface);
    raxel_key_callback_t key_callback = {.key = RAXEL_KEY_A, .on_button = on_key};
    raxel_input_manager_add_button_callback(input_manager, key_callback);

    // Create the pipeline.
    raxel_pipeline_t *pipeline = raxel_pipeline_create(&allocator, surface);
    surface->user_data = pipeline;

    // Initialize the pipeline (creates instance, device, swapchain, etc.).
    raxel_pipeline_initialize(pipeline);

    // Set the debug target to the internal color target.
    raxel_pipeline_set_debug_target(pipeline, RAXEL_PIPELINE_TARGET_COLOR);

    // Create a clear pass to clear the internal color target.
    raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.0f, 0.0f, 1.0f});
    raxel_pipeline_add_pass(pipeline, clear_pass);

    // --- Create and fill a dummy voxel world storage buffer ---
    // For simplicity, we create an array of 1024 uints representing voxel data.
    // In a real application, this data would come from your voxel world,
    // which ensures that loaded chunks are contiguous.
    const uint32_t voxel_buffer_size = 1024;
    uint32_t *voxel_data = malloc(voxel_buffer_size * sizeof(uint32_t));
    for (uint32_t i = 0; i < voxel_buffer_size; i++) {
        // Random material value between 0 and 255.
        voxel_data[i] = rand() % 256;
    }

    // --- Create the compute shader pass for raymarching ---
    // Assume the compiled SPIR-V file is at "internal/shaders/raymarch.comp.spv".
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/voxel.comp.spv");

    // Set up the push constant buffer.
    // Push constants: view matrix (16 floats), fov (1 float), and rays_per_pixel (1 int).
    raxel_pc_buffer_desc_t pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){.name = "view", .offset = 0, .size = 16 * sizeof(float)},
        (raxel_pc_entry_t){.name = "fov", .offset = 16 * sizeof(float), .size = sizeof(float)},
        (raxel_pc_entry_t){.name = "rays_per_pixel", .offset = 16 * sizeof(float) + sizeof(float), .size = sizeof(int)});
    raxel_compute_shader_set_pc(compute_shader, &pc_desc);

    // Set up the storage buffer for voxel data.
    raxel_sb_buffer_desc_t sb_desc = RAXEL_SB_DESC(
        (raxel_sb_entry_t){.name = "voxel_data", .offset = 0, .size = voxel_buffer_size * sizeof(uint32_t)});
    // This call will create the buffer and update binding 1 of the compute shader’s descriptor set.
    raxel_compute_shader_set_sb(compute_shader, pipeline, &sb_desc);

    // Copy our dummy voxel data into the storage buffer.
    memcpy(compute_shader->sb_buffer->data, voxel_data, voxel_buffer_size * sizeof(uint32_t));
    raxel_sb_buffer_update(compute_shader->sb_buffer, pipeline);
    // free(voxel_data);  // Data is now on the GPU.

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
    compute_ctx->pc_desc = pc_desc;
    compute_ctx->on_dispatch_finished = NULL;

    // Create the compute pass and add it to the pipeline.
    raxel_pipeline_pass_t compute_pass = raxel_compute_pass_create(compute_ctx);
    raxel_pipeline_add_pass(pipeline, compute_pass);

    raxel_pipeline_start(pipeline);

    // --- Main Loop ---
    // For demonstration, we update the push constants per frame.
    double time = 0.0;
    double delta_time = 0.01;
    vec3 camera_position = {0.0f, 0.0f, 0.0f};

    while (!raxel_pipeline_should_close(pipeline)) {
        raxel_pipeline_update(pipeline);
        time += delta_time;

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

        RAXEL_APP_LOG("Camera position: (%f, %f, %f)\n", camera_position[0], camera_position[1], camera_position[2]);

        // Update the view matrix (for simplicity, an identity matrix here).
        mat4 view;
        glm_mat4_identity(view);

        // Update the camera position.
        glm_translate(view, camera_position);


        // You could add camera rotation/translation here.
        raxel_pc_buffer_set(compute_shader->pc_buffer, "view", view);

        // Update fov: oscillate between 0.8 and 1.2.
        float fov = 90;
        raxel_pc_buffer_set(compute_shader->pc_buffer, "fov", &fov);

        // Update rays_per_pixel (for example, 4 rays per pixel).
        int rpp = 4;
        raxel_pc_buffer_set(compute_shader->pc_buffer, "rays_per_pixel", &rpp);
    }

    return 0;
}
