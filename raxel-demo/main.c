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
    raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.0f, 0.8f, 1.0f});
    raxel_pipeline_add_pass(pipeline, clear_pass);

    // --- Create and populate a voxel world ---
    raxel_voxel_world_t *world = raxel_voxel_world_create(&allocator);

    // For this test, we load one chunk.
    // Set the number of loaded chunks to 1.
    world->__num_loaded_chunks = 1;

    // Create a chunk covering world coordinates [0, 31] in x, y, z.
    raxel_voxel_chunk_t chunk;
    memset(&chunk, 0, sizeof(chunk));  // initially all voxels are air (0)

    // Set chunk metadata to indicate this chunk is at the origin.
    raxel_voxel_chunk_meta_t meta;
    meta.x = 0;
    meta.y = 0;
    meta.z = 0;
    meta.state = RAXEL_VOXEL_CHUNK_STATE_COUNT;  // not used atm

    // Add the meta and chunk to the world.
    raxel_list_push_back(world->chunk_meta, meta);
    raxel_list_push_back(world->chunks, chunk);

    // --- Build a sphere in the middle of the loaded chunk ---
    // Define sphere parameters in world space.
    // Since our chunk covers 0..31, choose center at (16,16,16) and radius = 10.
    vec3 sphereCenter = {0.0f, 0.0f, 0.0f};
    float sphereRadius = 10.0f;
    float sphereRadiusSq = sphereRadius * sphereRadius;

    // For every voxel coordinate within this chunk, compute world position and set material.
    // We'll fill voxels inside the sphere with material value 1.
    for (int x = 0; x < RAXEL_VOXEL_CHUNK_SIZE; x++) {
        for (int y = 0; y < RAXEL_VOXEL_CHUNK_SIZE; y++) {
            for (int z = 0; z < RAXEL_VOXEL_CHUNK_SIZE; z++) {
                // Compute world-space position of the voxel.
                // Since the chunk's origin is at (0,0,0), the world coordinate is just the index.
                float wx = (float)x;
                float wy = (float)y;
                float wz = (float)z;
                // Compute squared distance from sphere center.
                float dx = wx - sphereCenter[0];
                float dy = wy - sphereCenter[1];
                float dz = wz - sphereCenter[2];
                float distSq = dx * dx + dy * dy + dz * dz;
                raxel_voxel_t voxel = {0};
                if (distSq < sphereRadiusSq) {
                    // Set material to 1 (nonzero, meaning solid)
                    voxel.material = 1;
                }

                raxel_voxel_world_place_voxel(world, x, y, z, voxel);
            }
        }
    }

    // Optionally, update the voxel world based on camera position if needed:
    // raxel_voxel_world_update(world, &options);
    // For now, we assume our sphere is static.

    // --- Create the compute shader pass for voxel raymarching ---
    // Assume the compiled SPIR-V file is at "internal/shaders/voxel.comp.spv".
    // Set up the push constant buffer.
    // Push constants: view matrix (16 floats), fov (1 float), and rays_per_pixel (1 int).
    raxel_pc_buffer_desc_t pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){.name = "view", .offset = 0, .size = 16 * sizeof(float)},
        (raxel_pc_entry_t){.name = "fov", .offset = 16 * sizeof(float), .size = sizeof(float)},
        (raxel_pc_entry_t){.name = "rays_per_pixel", .offset = 16 * sizeof(float) + sizeof(float), .size = sizeof(int)});
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

    raxel_voxel_world_set_sb(world, compute_shader, pipeline);
    raxel_voxel_world_dispatch_sb(world, compute_shader, pipeline);

    raxel_pipeline_start(pipeline);

    // --- Main Loop ---
    // For demonstration, we update the push constants per frame.
    double time = 0.0;
    double delta_time = 0.01;
    vec3 camera_position = {0.0f, 0.0f, 0.0f};

    while (!raxel_pipeline_should_close(pipeline)) {
        raxel_pipeline_update(pipeline);
        time += delta_time;

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

        RAXEL_APP_LOG("Camera position: (%f, %f, %f)\n", camera_position[0], camera_position[1], camera_position[2]);

        // Update the view matrix.
        mat4 view;
        glm_mat4_identity(view);
        // Translate the view matrix by the negative camera position.
        // (Depending on your convention, you might need to invert this translation.)
        glm_translate(view, camera_position);
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
        options.view_distance = 10.0f;
    }

    return 0;
}
