#ifndef __COMPUTE_PASS_H__
#define __COMPUTE_PASS_H__

#include <raxel/core/graphics.h>
#include <raxel/core/util.h>
#include <cglm/cglm.h>

typedef enum raxel_compute_descriptor_binding {
    RAXEL_COMPUTE_BINDING_STORAGE_IMAGE = 0,
    RAXEL_COMPUTE_BINDING_STORAGE_BUFFER = 1,
    RAXEL_COMPUTE_BINDING_COUNT
} raxel_compute_descriptor_binding;

typedef struct raxel_compute_shader {
    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;
    VkDescriptorSet descriptor_set; // Bound to a storage image (set=0, binding=0)
    raxel_pc_buffer_t *pc_buffer;
    raxel_sb_buffer_t *sb_buffer;
    raxel_allocator_t *allocator;
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

/**
 * Create a push–constant buffer descriptor for a compute shader.
 *
 * @param shader Pointer to the compute shader.
 * @param desc Descriptor for the push–constant buffer.
 */
void raxel_compute_shader_set_pc(raxel_compute_shader_t *shader, raxel_pc_buffer_desc_t *desc);


/**
 * @brief Passes the push-constant buffer to the compute shader.
 * 
 * @param shader Pointer to the compute shader.
 * @param cmd_buffer The command buffer to record the push-constant update.
 */
void raxel_compute_shader_push_pc(raxel_compute_shader_t *shader, VkCommandBuffer cmd_buffer);


/**
 * @brief Passes the storage buffer to the compute shader.
 * 
 * @param shader Pointer to the compute shader.
 * @param desc Descriptor for the storage buffer.
 */
void raxel_compute_shader_set_sb(raxel_compute_shader_t *shader, raxel_pipeline_t *pipeline, raxel_sb_buffer_desc_t *desc);



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
    void (*on_dispatch_finished)(struct raxel_compute_pass_context *context, raxel_pipeline_globals_t *globals);
} raxel_compute_pass_context_t;

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
