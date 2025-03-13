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

// Called when the surface is destroyed.
static void on_destroy(raxel_surface_t *surface) {
    RAXEL_CORE_LOG("Destroying surface...\n");
    raxel_pipeline_t *pipeline = (raxel_pipeline_t *)surface->user_data;
    raxel_pipeline_cleanup(pipeline);
    raxel_pipeline_destroy(pipeline);
}

// Define a GPU-side structure matching what we will send to the shader.
typedef struct GPUVoxelWorld {
    uint32_t num_loaded_chunks;
    raxel_voxel_chunk_meta_t chunk_meta[RAXEL_MAX_LOADED_CHUNKS];
    raxel_voxel_chunk_t chunks[RAXEL_MAX_LOADED_CHUNKS];
} GPUVoxelWorld;

int main(void) {
    // Initialize random seed.
    srand((unsigned)time(NULL));

    // Create a default allocator.
    raxel_allocator_t allocator = raxel_default_allocator();

    // Create a window and Vulkan surface.
    raxel_surface_t *surface = raxel_surface_create(&allocator, "Voxel Raymarch", WIDTH, HEIGHT);

    // Setup input
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

    // --- Create and populate a dummy voxel world ---
    raxel_voxel_world_t *world = raxel_voxel_world_create(&allocator);

    // For this test, we load a few chunks (up to RAXEL_MAX_LOADED_CHUNKS).
    // Here we simply create, say, 4 chunks with random voxel data.
    world->__num_loaded_chunks = 4;
    for (uint32_t i = 0; i < world->__num_loaded_chunks; i++) {
        // For simplicity, set chunk metadata (positioned in a grid).
        raxel_voxel_chunk_meta_t meta = { .x = i, .y = 0, .z = 0, .state = RAXEL_VOXEL_CHUNK_STATE_COUNT };
        raxel_list_push_back(world->chunk_meta, meta);
        // Create a new chunk with random voxel values.
        raxel_voxel_chunk_t chunk;
        for (int x = 0; x < RAXEL_VOXEL_CHUNK_SIZE; x++) {
            for (int y = 0; y < RAXEL_VOXEL_CHUNK_SIZE; y++) {
                for (int z = 0; z < RAXEL_VOXEL_CHUNK_SIZE; z++) {
                    // assign half of them to be solid (1) and the rest empty (0)
                    chunk.voxels[x][y][z].material = rand() % 2;
                }
            }
        }
        raxel_list_push_back(world->chunks, chunk);
    }

    // (For a real application, you might also fill the materials list.)

    // --- Flatten the voxel world data into a contiguous block for the GPU ---
    GPUVoxelWorld gpuWorld = {0};
    gpuWorld.num_loaded_chunks = (uint32_t)world->__num_loaded_chunks;
    for (uint32_t i = 0; i < gpuWorld.num_loaded_chunks; i++) {
        // Copy chunk metadata.
        gpuWorld.chunk_meta[i] = world->chunk_meta[i];
        // Copy chunk data.
        gpuWorld.chunks[i] = world->chunks[i];
    }

    // --- Create the compute shader pass for voxel raymarching ---
    // Assume the compiled SPIR-V file is at "internal/shaders/voxel.comp.spv".
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/voxel.comp.spv");

    // Set up the push constant buffer.
    // Push constants: view matrix (16 floats), fov (1 float), and rays_per_pixel (1 int).
    raxel_pc_buffer_desc_t pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){ .name = "view", .offset = 0, .size = 16 * sizeof(float) },
        (raxel_pc_entry_t){ .name = "fov", .offset = 16 * sizeof(float), .size = sizeof(float) },
        (raxel_pc_entry_t){ .name = "rays_per_pixel", .offset = 16 * sizeof(float) + sizeof(float), .size = sizeof(int) }
    );
    raxel_compute_shader_set_pc(compute_shader, &pc_desc);

    // Set up the storage buffer for the voxel world data.
    // The size of our data is the size of GPUVoxelWorld.
    raxel_sb_buffer_desc_t sb_desc = RAXEL_SB_DESC(
        (raxel_sb_entry_t){ .name = "voxel_world", .offset = 0, .size = sizeof(GPUVoxelWorld) }
    );
    // This call will create the buffer and update binding 1 of the compute shader’s descriptor set.
    raxel_compute_shader_set_sb(compute_shader, pipeline, &sb_desc);

    // Copy our flattened voxel world into the storage buffer.
    memcpy(compute_shader->sb_buffer->data, &gpuWorld, sizeof(GPUVoxelWorld));
    raxel_sb_buffer_update(compute_shader->sb_buffer, pipeline);

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

        // Simple WASD controls to move the camera.
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

        // Update the view matrix. For simplicity, start with an identity matrix and translate.
        mat4 view;
        glm_mat4_identity(view);
        glm_translate(view, camera_position);
        raxel_pc_buffer_set(compute_shader->pc_buffer, "view", view);

        // Update fov (e.g., 90 degrees).
        float fov = 90.0f;
        raxel_pc_buffer_set(compute_shader->pc_buffer, "fov", &fov);

        // Update rays per pixel (e.g., 4 rays per pixel).
        int rpp = 4;
        raxel_pc_buffer_set(compute_shader->pc_buffer, "rays_per_pixel", &rpp);
    }

    return 0;
}
