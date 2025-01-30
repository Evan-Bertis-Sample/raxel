#ifndef __RAXEL_DEBUG_H__
#define __RAXEL_DEBUG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>   // for printf()
#include <stdlib.h>  // for exit()
#include <string.h>  // for strrchr()

// Example toggles
#define RAXEL_CORE_LOG_ENABLED
#define RAXEL_CORE_ALLOW_FATAL_ERRORS  // if defined, will call exit() on fatal errors
#define RAXEL_APP_LOG_ENABLED
#define RAXEL_APP_ALLOW_FATAL_ERRORS

#define RAXEL_STR_HELPER(x) #x
#define RAXEL_STR(x) RAXEL_STR_HELPER(x)

#ifdef __clang__
// Clang-specific: __FILE_NAME__ is already the stripped filename
#define RAXEL_FILENAME __FILE_NAME__
#elif defined(__GNUC__)
// Evaluate at runtime (technically, it can be constant-folded):
#define RAXEL_FILENAME                         \
    (__builtin_strrchr(__FILE__, '/')           \
         ? __builtin_strrchr(__FILE__, '/') + 1 \
         : __FILE__)

#else
// Fallback for other compilers
#define RAXEL_FILENAME __FILE__
#endif

// If you still want a macro for the line:
#define RAXEL_LINE __LINE__

// Some color codes for console printing
#define RAXEL_PREFIX_COLOR "\033[1;34m"
#define RAXEL_ERROR_COLOR "\033[1;31m"
#define RAXEL_PLATFORM_COLOR "\033[1;32m"
#define RAXEL_APP_COLOR "\033[1;33m"
#define RAXEL_SUFFIX_COLOR "\033[0m"

// Just two prefix strings (no path/line in them)
// We'll insert [%s:%d] at runtime in the macros
#define RAXEL_CORE_MSG_PREFIX "[raxel-core]"

#define RAXEL_APP_MSG_PREFIX "[raxel-app]"

#ifdef RAXEL_CORE_LOG_ENABLED

// Example:
// [raxel-core] [myfile.c:42] some message
#define RAXEL_CORE_LOG(fmt, ...)                                                         \
    printf(RAXEL_PREFIX_COLOR RAXEL_CORE_MSG_PREFIX "[%s:%d] " RAXEL_SUFFIX_COLOR fmt, \
           RAXEL_FILENAME, RAXEL_LINE, ##__VA_ARGS__)

#define RAXEL_CORE_LOG_ERROR(fmt, ...)                                                              \
    printf(RAXEL_ERROR_COLOR RAXEL_CORE_MSG_PREFIX "[%s:%d][ERROR] " RAXEL_SUFFIX_COLOR fmt, \
           RAXEL_FILENAME, RAXEL_LINE, ##__VA_ARGS__)
#else
#define RAXEL_CORE_LOG(...) (void)0
#define RAXEL_CORE_LOG_ERROR(...) (void)0
#endif

#ifdef RAXEL_CORE_ALLOW_FATAL_ERRORS
#define RAXEL_CORE_FATAL_ERROR(...)        \
    {                                       \
        RAXEL_CORE_LOG_ERROR(__VA_ARGS__); \
        exit(1);                            \
    }
#else
#define RAXEL_CORE_FATAL_ERROR(...) RAXEL_CORE_LOG_ERROR(__VA_ARGS__)
#endif

#ifdef RAXEL_APP_LOG_ENABLED

// Example:
// [RAXEL-app][myfile.c:42] some message
#define RAXEL_APP_LOG(fmt, ...)                                                      \
    printf(RAXEL_APP_COLOR RAXEL_APP_MSG_PREFIX "[%s:%d] " RAXEL_SUFFIX_COLOR fmt, \
           RAXEL_FILENAME, RAXEL_LINE, ##__VA_ARGS__)

#define RAXEL_APP_LOG_ERROR(fmt, ...)                                                \
    printf(RAXEL_APP_COLOR RAXEL_APP_MSG_PREFIX "[%s:%d] " RAXEL_SUFFIX_COLOR fmt, \
           RAXEL_FILENAME, RAXEL_LINE, ##__VA_ARGS__)

#else
#define RAXEL_APP_LOG(...) (void)0
#define RAXEL_APP_LOG_ERROR(...) (void)0
#endif

#ifdef RAXEL_APP_ALLOW_FATAL_ERRORS
#define RAXEL_APP_FATAL_ERROR(...)        \
    {                                      \
        RAXEL_APP_LOG_ERROR(__VA_ARGS__); \
        exit(1);                           \
    }
#else
#define RAXEL_APP_FATAL_ERROR(...) RAXEL_APP_LOG_ERROR(__VA_ARGS__)
#endif

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif // __RAXEL_DEBUG_H__