#include <cglm/cglm.h>
#include <math.h>
#include <raxel/core/graphics.h>
#include <raxel/core/graphics/passes/clear_color_pass.h>
#include <raxel/core/graphics/passes/compute_pass.h>
#include <raxel/core/input.h>
#include <raxel/core/util.h>
#include <stdint.h>

#define WIDTH 800
#define HEIGHT 600

static void on_key(raxel_key_event_t event) {
    RAXEL_CORE_LOG("Pushed key %d\n", event.key);
}

static void on_destroy(raxel_surface_t *surface) {
    RAXEL_CORE_LOG("Destroying surface\n");
    raxel_pipeline_t *pipeline = (raxel_pipeline_t *)surface->user_data;
    raxel_pipeline_cleanup(pipeline);
    raxel_pipeline_destroy(pipeline);
}

int main(void) {
    // Get a default allocator.
    raxel_allocator_t allocator = raxel_default_allocator();
    // Create a surface (this call creates a window and the associated Vulkan surface).
    raxel_surface_t *surface = raxel_surface_create(&allocator, "Voxel", WIDTH, HEIGHT);

    RAXEL_APP_LOG("Creating input manager\n");
    raxel_input_manager_t *input_manager = raxel_input_manager_create(&allocator, surface);
    raxel_key_callback_t key_callback = {
        .key = RAXEL_KEY_A,
        .on_button = on_key,
    };
    RAXEL_APP_LOG("Adding key callback\n");
    raxel_input_manager_add_button_callback(input_manager, key_callback);

    RAXEL_APP_LOG("Initializing surface\n");

    // Create the pipeline with the surface.
    raxel_pipeline_t *pipeline = raxel_pipeline_create(&allocator, surface);
    surface->user_data = pipeline;

    RAXEL_APP_LOG("Initializing pipeline\n");
    // Initialize the pipeline (this creates the Vulkan instance, device, swapchain, internal targets, sync objects, etc.).
    raxel_pipeline_initialize(pipeline);

    RAXEL_APP_LOG("Setting debug target\n");
    // Set the debug target to the internal color target.
    raxel_pipeline_set_debug_target(pipeline, RAXEL_PIPELINE_TARGET_COLOR);

    // Create the first pass: a clear-color pass that clears the internal color target to blue.
    raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.0f, 1.0f, 1.0f});
    raxel_pipeline_add_pass(pipeline, clear_pass);

    // Create a compute shader.
    // For this example, the shader outputs UV coordinates and also uses voxel data.
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/voxel.comp.spv");

    // Set up the push constant buffer.
    raxel_pc_buffer_desc_t pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){
            .name = "view",
            .offset = 0,
            .size = 16 * sizeof(float),
        },
        (raxel_pc_entry_t){
            .name = "fov",
            .offset = 16 * sizeof(float),
            .size = sizeof(float),
        });
    raxel_compute_shader_set_pc(compute_shader, &pc_desc);

    // Now, set up a storage buffer for voxel data.
    // Let's assume we want 1024 voxels.
    uint32_t voxel_count = 1024;
    raxel_sb_buffer_desc_t sb_desc = RAXEL_SB_DESC(
        (raxel_sb_entry_t){.name = "voxels", .offset = 0, .size = voxel_count * sizeof(uint32_t)});
    // Attach the storage buffer (assumes binding 1 is reserved for the storage buffer).
    raxel_compute_shader_set_sb(compute_shader, pipeline, &sb_desc);

    // Initialize the voxel data.
    uint32_t *voxels = (uint32_t *)compute_shader->sb_buffer->data;
    for (uint32_t i = 0; i < voxel_count; i++) {
        voxels[i] = i % 256;
    }

    // Create a compute pass context.
    raxel_compute_pass_context_t *compute_ctx = raxel_malloc(&allocator, sizeof(raxel_compute_pass_context_t));
    compute_ctx->compute_shader = compute_shader;
    // Set dispatch dimensions.
    compute_ctx->dispatch_x = (WIDTH + 15) / 16;
    compute_ctx->dispatch_y = (HEIGHT + 15) / 16;
    compute_ctx->dispatch_z = 1;
    // Set the blit target to the internal color target.
    compute_ctx->targets[0] = RAXEL_PIPELINE_TARGET_COLOR;
    compute_ctx->targets[1] = -1;  // Signal the end of the array.
    compute_ctx->pc_desc = pc_desc;
    compute_ctx->on_dispatch_finished = NULL;

    // Create the compute pass and add it to the pipeline.
    raxel_pipeline_pass_t compute_pass = raxel_compute_pass_create(compute_ctx);
    raxel_pipeline_add_pass(pipeline, compute_pass);

    raxel_pipeline_start(pipeline);

    double time = 0.0;
    double delta_time = 0.01;
    while (!raxel_pipeline_should_close(pipeline)) {
        raxel_pipeline_update(pipeline);
        time += delta_time;

        // Update the fov value in the push constant buffer.
        float fov = 1.0f + sin(time) * 0.5f;
        raxel_pc_buffer_set(compute_ctx->compute_shader->pc_buffer, "fov", &fov);

        // Optionally update voxel data per frame.
        // For example, cycle voxel values based on time.
        for (uint32_t i = 0; i < voxel_count; i++) {
            // RAXEL_CORE_LOG("Updating voxel %d - %d\n", i, (i + (uint32_t)(time * 10)) % 256);
            voxels[i] = (i + (uint32_t)(time * 10)) % 256;
        }

        // RAXEL_APP_LOG("Updating SB!\n");
        raxel_sb_buffer_update(compute_ctx->compute_shader->sb_buffer, pipeline);
    }

    return 0;
}
