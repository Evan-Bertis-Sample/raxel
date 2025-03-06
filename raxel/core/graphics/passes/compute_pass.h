#ifndef __COMPUTE_PASS_H__
#define __COMPUTE_PASS_H__

#include <raxel/core/graphics.h>
#include <cglm/cglm.h>

// -----------------------------------------------------------------------------
// Compute Shader Abstraction
// -----------------------------------------------------------------------------

typedef struct raxel_push_constant_entry {
    char *name;      // e.g. "view" or "fov"
    size_t offset;   // Offset into the push constant buffer.
    size_t size;     // Size of this constant.
} raxel_push_constant_entry_t;

typedef struct raxel_compute_shader {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSet descriptor_set; // Optional.
    raxel_list(raxel_push_constant_entry_t) __push_layout; // Private push constant layout.
    void *push_data;      // Buffer holding push constant data.
    size_t push_data_size; // Total size.
} raxel_compute_shader_t;

/**
 * Create a compute shader from a SPIR-V binary.
 * The pipeline pointer is used to get Vulkan resources.
 *
 * @param pipeline Pointer to the pipeline.
 * @param shader_path File path to the SPIR-V compute shader.
 * @param push_data_size Total size for the push constant buffer.
 * @return Pointer to a newly created compute shader.
 */
raxel_compute_shader_t *raxel_compute_shader_create(raxel_pipeline_t *pipeline, const char *shader_path, size_t push_data_size);

/**
 * Destroy a compute shader.
 *
 * @param shader Pointer to the compute shader.
 * @param pipeline Pointer to the pipeline.
 */
void raxel_compute_shader_destroy(raxel_compute_shader_t *shader, raxel_pipeline_t *pipeline);

/**
 * Push a constant value by name into the compute shader’s push constant buffer.
 *
 * @param shader Pointer to the compute shader.
 * @param name The name of the push constant.
 * @param data Pointer to the data.
 */
void raxel_compute_shader_push_constant(raxel_compute_shader_t *shader, const char *name, const void *data);

// -----------------------------------------------------------------------------
// Compute Pass Context
// -----------------------------------------------------------------------------
typedef struct raxel_compute_pass_context {
    raxel_compute_shader_t *compute_shader;
    uint32_t dispatch_x;
    uint32_t dispatch_y;
    uint32_t dispatch_z;
    // Which target to blit from; for example, RAXEL_PIPELINE_TARGET_COLOR.
    raxel_pipeline_target_type_t blit_target;
    // Optional output image from the compute shader. If VK_NULL_HANDLE, then use the internal target.
    VkImage output_image;
    // Callback invoked after dispatch finishes. If NULL, the default blit is used.
    void (*on_dispatch_finished)(struct raxel_compute_pass_context *context, raxel_pipeline_t *pipeline);
} raxel_compute_pass_context_t;

/**
 * Default blit callback: Copies the computed result (source image) into the pipeline target.
 */
void raxel_compute_pass_blit(raxel_compute_pass_context_t *context, raxel_pipeline_t *pipeline);

// -----------------------------------------------------------------------------
// Compute Pass API
// -----------------------------------------------------------------------------
/**
 * Create a compute pass using the provided compute pass context.
 *
 * @param context Pointer to a compute pass context.
 * @return A raxel_pipeline_pass_t representing the compute pass.
 */
raxel_pipeline_pass_t raxel_compute_pass_create(raxel_compute_pass_context_t *context);

#endif // __COMPUTE_PASS_H__
