#ifndef __COMPUTE_PASS_H__
#define __COMPUTE_PASS_H__

#include <raxel/core/graphics.h>
#include <raxel/core/util.h>
#include <cglm/cglm.h>


typedef struct raxel_compute_shader {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSet descriptor_set; // Bound to a storage image (set=0, binding=0)
    raxel_pc_buffer_t push_constants;
} raxel_compute_shader_t;

/**
 * Create a compute shader from a SPIR-V binary.
 * Uses the pipeline’s device.
 *
 * @param pipeline Pointer to the pipeline.
 * @param shader_path File path to the SPIR-V compute shader.
 * @return Pointer to a newly created compute shader.
 */
raxel_compute_shader_t *raxel_compute_shader_create(raxel_pipeline_t *pipeline, const char *shader_path);

/**
 * Destroy a compute shader.
 *
 * @param shader Pointer to the compute shader.
 * @param pipeline Pointer to the pipeline.
 */
void raxel_compute_shader_destroy(raxel_compute_shader_t *shader, raxel_pipeline_t *pipeline);

// -----------------------------------------------------------------------------
// Compute Pass Context
// -----------------------------------------------------------------------------
typedef struct raxel_compute_pass_context {
    raxel_compute_shader_t *compute_shader;
    uint32_t dispatch_x;
    uint32_t dispatch_y;
    uint32_t dispatch_z;
    // Which internal target to use for blitting the compute result.
    raxel_pipeline_target_type_t targets[RAXEL_PIPELINE_TARGET_COUNT];
    raxel_pc_buffer_desc_t pc_desc;
    // Optional: if non-null, use this image as the computed result.
    VkImage output_image;
    // Callback invoked after dispatch finishes.
    // If NULL, the default blit callback is used.
    VkDescriptorImageInfo *image_infos;
    raxel_size_t num_image_infos;
    void (*on_dispatch_finished)(struct raxel_compute_pass_context *context, raxel_pipeline_t *pipeline);
} raxel_compute_pass_context_t;

/**
 * Default blit callback: Blit the computed result into the pipeline target.
 * This function copies from the compute output (either context->output_image, or the
 * internal target specified by blit_target) into the pipeline's target image.
 * Presentation is handled later by the pipeline's present() function.
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
