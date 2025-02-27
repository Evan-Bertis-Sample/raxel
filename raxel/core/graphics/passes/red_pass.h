#ifndef RED_PASS_H
#define RED_PASS_H

#include <raxel/core/graphics.h>

/**
 * Create a red pass that clears the chosen target to red.
 *
 * The pass's on_begin callback records a command buffer to clear the
 * first swapchain image (index 0) to red.
 *
 * @return A raxel_pipeline_pass_t representing the red clearing pass.
 */
raxel_pipeline_pass_t red_pass_create(void);

#endif // RED_PASS_H
