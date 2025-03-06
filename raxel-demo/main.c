#include <raxel/core/util.h>
#include <raxel/core/graphics.h>
#include <raxel/core/graphics/passes/clear_color_pass.h>
#include <raxel/core/graphics/passes/compute_pass.h>
#include <cglm/cglm.h>

#define WIDTH  800
#define HEIGHT 600

int main(void) {
    // Create a surface (this call creates a window and the associated Vulkan surface).
    raxel_surface_t surface = raxel_surface_create("UV Compute", WIDTH, HEIGHT);
    
    // Get a default allocator.
    raxel_allocator_t allocator = raxel_default_allocator();
    
    // Create the pipeline with the surface.
    raxel_pipeline_t *pipeline = raxel_pipeline_create(&allocator, surface);
    
    // Initialize the pipeline (this creates the Vulkan instance, device, swapchain, internal targets, sync objects, etc.).
    raxel_pipeline_initialize(pipeline);
    
    // Set the debug target to the internal color target.
    raxel_pipeline_set_debug_target(pipeline, RAXEL_PIPELINE_TARGET_COLOR);
    
    // Create a compute shader using our compute shader abstraction.
    // For this example, we use a shader that outputs UV coordinates (normalized pixel coordinates).
    // We set the push constant size to 80 bytes (matching our earlier example).
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/uv.comp.spv");
    
    // Create a compute pass context.
    raxel_compute_pass_context_t compute_ctx = {0};

    compute_ctx.dispatch_x = (WIDTH + 15) / 16;
    compute_ctx.compute_shader = compute_shader;
    compute_ctx.dispatch_y = (HEIGHT + 15) / 16;
    compute_ctx.dispatch_z = 1;
    // Set the blit target to the color target.
    compute_ctx.blit_target = RAXEL_PIPELINE_TARGET_COLOR;
    // Use default on_dispatch_finished (which calls raxel_pipeline_present)
    compute_ctx.on_dispatch_finished = NULL;
    
    // Create the compute pass.
    raxel_pipeline_pass_t compute_pass = raxel_compute_pass_create(&compute_ctx);
    // Add the compute pass to the pipeline.
    raxel_pipeline_add_pass(pipeline, compute_pass);
    
    // Optionally, you might add a clear color pass to initialize the internal buffer to a known state.
    // For example:
    // raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.0f, 0.0f, 1.0f});
    // raxel_pipeline_add_pass(pipeline, clear_pass);
    
    // Run the pipeline main loop.
    raxel_pipeline_run(pipeline);
    
    // Cleanup after the main loop exits.
    raxel_pipeline_cleanup(pipeline);
    
    return 0;
}
