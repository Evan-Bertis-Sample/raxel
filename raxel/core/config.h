#ifndef __CONFIG_H__
#define __CONFIG_H__

// just a bunch of #defines to control compilation


/**------------------------------------------------------------------------
 *                           DEBUGGING FLAGS
 *------------------------------------------------------------------------**/

#define RAXEL_CORE_LOG_ENABLED
#define RAXEL_CORE_ALLOW_FATAL_ERRORS  // if defined, will call exit() on fatal errors
#define RAXEL_APP_LOG_ENABLED
#define RAXEL_APP_ALLOW_FATAL_ERRORS

/**------------------------------------------------------------------------
 *                           GRAPHICS FLAGSS
 *------------------------------------------------------------------------**/

#define RAXEL_SURFACE_USE_GLFW

#endif // __CONFIG_H__