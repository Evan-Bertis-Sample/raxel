# PC (Push Constants)

## "Game" Code
```c
#include <raxel/core/util.h>
#include <raxel/core/graphics.h>
#include <raxel/core/input.h>
#include <raxel/core/graphics/passes/clear_color_pass.h>
#include <raxel/core/graphics/passes/compute_pass.h>
#include <cglm/cglm.h>

#define WIDTH  800
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
    raxel_surface_t *surface = raxel_surface_create(&allocator, "UV Compute", WIDTH, HEIGHT);

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

    // Create the first pass in the pipeline: a clear-color pass that clears the internal color target to blue.
    raxel_pipeline_pass_t clear_pass = clear_color_pass_create((vec4){0.0f, 0.0f, 1.0f, 1.0f});
    raxel_pipeline_add_pass(pipeline, clear_pass);
    
    // Create a compute shader using our compute shader abstraction.
    // BTW I really like this API -- it mirrors Unity's ComputeShader API, which
    // honestly is one of the best parts of Unity.
    // For this example, we use a shader that outputs UV coordinates (normalized pixel coordinates).
    raxel_compute_shader_t *compute_shader = raxel_compute_shader_create(pipeline, "internal/shaders/uv.comp.spv");
    // Create a compute pass context.
    raxel_compute_pass_context_t *compute_ctx = raxel_malloc(&allocator, sizeof(raxel_compute_pass_context_t));
    // pass the compute shader to the context
    compute_ctx->compute_shader = compute_shader;
    // set some options about dispatching the compute shader
    compute_ctx->dispatch_x = (WIDTH + 15) / 16;
    compute_ctx->dispatch_y = (HEIGHT + 15) / 16;
    compute_ctx->dispatch_z = 1;
    // Set the blit target to the color target.
    compute_ctx->targets[0] = RAXEL_PIPELINE_TARGET_COLOR;
    compute_ctx->targets[1] = -1; // we only have one target, signal the end of the array
    // Describe the push constant buffer layout.
    compute_ctx->pc_desc = RAXEL_PC_DESC(
        (raxel_pc_entry_t){
            .name = "view",
            .offset = 0,
            .size = 16 * sizeof(float),
        },
        (raxel_pc_entry_t){
            .name = "fov",
            .offset = 16 * sizeof(float),
            .size = sizeof(float),
        }       
    );
    // we can attach a callback to be called after the compute shader dispatch finishes
    // for now we do nothing
    compute_ctx->on_dispatch_finished = NULL;

    // Create the compute pass!
    raxel_pipeline_pass_t compute_pass = raxel_compute_pass_create(compute_ctx);
    // Add the compute pass to the pipeline.
    raxel_pipeline_add_pass(pipeline, compute_pass);

    raxel_pipeline_start(pipeline);

    double time = 0.0;
    double delta_time = 0.01;
    while (!raxel_pipeline_should_close(pipeline)) {
        raxel_pipeline_update(pipeline);
        time += delta_time;

        // update the fov in the push constant buffer
        float fov = 1.0f + sin(time) * 0.5f;
        raxel_pc_buffer_set(compute_ctx->compute_shader->pc_buffer, "fov", &fov);
    }
    
    return 0;
}
```

## Shader Code
```glsl
#version 450
layout(local_size_x = 16, local_size_y = 16) in;

// Output image (set = 0, binding = 0)
layout(rgba32f, set = 0, binding = 0) uniform image2D outImage;

// Push constant block containing a view matrix and a modulation value.
layout(push_constant) uniform PushConstants {
    mat4 view;
    float fov;
} pc;

void main() {
    // Get the size of the output image and the current pixel coordinates.
    ivec2 imageSize = imageSize(outImage);
    ivec2 pixelCoords = ivec2(gl_GlobalInvocationID.xy);
    
    // Compute the normalized pixel coordinates.
    vec2 uv = vec2(pixelCoords) / vec2(imageSize);
    
    // Transform the uv coordinates with the push constant view matrix.
    // Here we treat uv as an XY position; the Z is zero and W is 1.
    vec4 transformed = pc.view * vec4(uv, 0.0, 1.0);
    
    // Use the fov push constant as a modulation factor.
    // Here we take its sine (absolute value) to get a value between 0 and 1.
    float modFactor = abs(sin(pc.fov));
    
    // Create a base color from the original uv coordinates.
    vec4 baseColor = vec4(uv, 0.0, 1.0);
    
    // Mix the base color with the color from the transformed coordinates.
    // The transformation may rotate or skew the uv, giving an interesting effect.
    vec4 newColor = mix(baseColor, vec4(transformed.xy, 0.0, 1.0), modFactor);
    
    // Add a conditional tweak: for pixels on the right half, darken; on the left, brighten.
    if (uv.x > 0.5) {
        newColor.rgb *= 0.7;
    } else {
        newColor.rgb *= 1.3;
    }
    
    // Write the final color to the output image.
    imageStore(outImage, pixelCoords, newColor);
}
```