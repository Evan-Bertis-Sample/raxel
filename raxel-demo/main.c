#include <raxel/core/util.h>
#include <raxel/core/graphics.h>
#include <raxel/core/graphics/passes/clear_color_pass.h>


int main(void) {
    // Create a surface (for example, with a 800x600 window).
    raxel_surface_t surface = raxel_surface_create("Red Screen", 800, 600);
    // In a real app, you would create the VkInstance first and pass it in.
    // Here, we assume raxel_surface_create internally creates the VkSurfaceKHR
    // based on an externally created instance.

    raxel_allocator_t allocator = raxel_default_allocator();

    // Create the pipeline.
    raxel_pipeline_t *pipeline = raxel_pipeline_create(&allocator, surface);

    // Initialize the pipeline (creates instance, device, swapchain, targets, etc.).
    raxel_pipeline_initialize(pipeline);

    // (Our pipeline_create_swapchain and create_targets are automatically called during initialization
    // if we update our implementation accordingly.)
    // For this example, assume they have been called.

    // Set the debug target to the color target.
    raxel_pipeline_set_debug_target(pipeline, RAXEL_PIPELINE_TARGET_COLOR);

    // Add our red pass to the pipeline.
    raxel_pipeline_pass_t color_pass = clear_color_pass_create((vec4){1.0f, 0.0f, 0.0f, 1.0f});
    
    raxel_pipeline_add_pass(pipeline, color_pass);

    // Run the pipeline main loop.
    raxel_pipeline_run(pipeline);

    // Cleanup after the main loop exits.
    raxel_pipeline_cleanup(pipeline);

    return 0;
}
