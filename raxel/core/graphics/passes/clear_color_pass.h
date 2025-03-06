#ifndef __CLEAR_COLOR_PASS_H__
#define __CLEAR_COLOR_PASS_H__

#include <cglm/cglm.h>
#include <raxel/core/graphics.h>

/**
 * Create a clear-color pass that clears the internal color target to the specified color.
 *
 * @param clear_color A cglm vec4 (float[4]) representing the clear color.
 * @return A raxel_pipeline_pass_t representing the clear-color pass.
 */
raxel_pipeline_pass_t clear_color_pass_create(vec4 clear_color);

#endif // __CLEAR_COLOR_PASS_H__