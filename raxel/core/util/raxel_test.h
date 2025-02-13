#ifndef __RAXEL_TEST_H__
#define __RAXEL_TEST_H__

/*
 * Minimal test framework for RAXEl that uses raxel_debug.h for logging.
 * 
 * Usage:
 *   1) #include "raxel_debug.h"
 *   2) #include "raxel_test.h"
 *   3) Define tests with RAXEL_TEST(my_test_name) { ... }.
 *   4) Use RAXEL_TEST_ASSERT(expr) to check conditions.
 *   5) Add RAXEL_TEST_MAIN() at the end if you want a standalone main().
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>  // for exit()
#include "raxel_debug.h"  // For RAXEL_CORE_LOG, etc.

/* 
 * Each test case has:
 *  - A test function (no arguments, no return).
 *  - A name for reporting.
 */
typedef void (*raxel_test_fn_t)(void);

typedef struct raxel_test_case_s {
    const char      *name;
    raxel_test_fn_t  func;
} raxel_test_case_t;

/* 
 * We store test cases in a static array. 
 * (You can raise the 1024 limit if needed.)
 */
static raxel_test_case_t __raxel_test_cases[1024];
static size_t           __raxel_test_case_count = 0;

/*
 * If you prefer using the "app" macros for logging instead, 
 * you can define these to RAXEL_APP_LOG / RAXEL_APP_LOG_ERROR.
 */
#define RAXEL_TEST_LOG       RAXEL_CORE_LOG
#define RAXEL_TEST_LOG_ERROR RAXEL_CORE_LOG_ERROR
#define RAXEL_TEST_FATAL     RAXEL_CORE_FATAL_ERROR  /* or RAXEL_APP_FATAL_ERROR */

/*
 * Macro: RAXEL_TEST(name)
 * 
 * - Declares/defines a test function named `name`.
 * - Also registers it into the test case array at load time (GCC/Clang).
 * - On MSVC (or other compilers without __attribute__((constructor))),
 *   you might need a manual registration or some other approach.
 */
#ifdef _MSC_VER
/* 
 * For MSVC, we do not have __attribute__((constructor)).
 * We'll define a placeholder macro. You must call raxel_register_test()
 * yourself in your main or test setup code.
 */
#define RAXEL_TEST(name) \
    static void name(void); \
    /* Example usage in main: raxel_register_test(#name, name); */ \
    static void name(void)

static void raxel_register_test(const char *test_name, raxel_test_fn_t fn) {
    __raxel_test_cases[__raxel_test_case_count].name = test_name;
    __raxel_test_cases[__raxel_test_case_count].func = fn;
    __raxel_test_case_count++;
}

#else /* GCC/Clang version */

#define RAXEL_TEST(name)                                               \
    static void name(void);                                            \
    static void __register_##name(void) __attribute__((constructor));  \
    static void __register_##name(void) {                              \
        __raxel_test_cases[__raxel_test_case_count].name = #name;      \
        __raxel_test_cases[__raxel_test_case_count].func = name;       \
        __raxel_test_case_count++;                                     \
    }                                                                  \
    static void name(void)

#endif /* _MSC_VER */

/*
 * Macro: RAXEL_TEST_ASSERT(expr)
 * 
 * - Checks `expr`; if false, logs an error (colored) and exits.
 * - If you do not want to exit on failure, replace RAXEL_TEST_FATAL.
 */
#define RAXEL_TEST_ASSERT(expr)                                             \
    do {                                                                    \
        if (!(expr)) {                                                      \
            RAXEL_TEST_FATAL("Assertion failed in test '%s': (%s)\n",       \
                             __func__, #expr);                              \
        }                                                                   \
    } while (0)

/*
 * Runs all registered tests in the __raxel_test_cases array.
 * Returns 0 if successful (i.e. no test calls exit).
 */
static int raxel_run_all_tests(void) {
    RAXEL_TEST_LOG("Running %zu test(s)...\n", __raxel_test_case_count);

    for (size_t i = 0; i < __raxel_test_case_count; i++) {
        RAXEL_TEST_LOG("  Test #%zu: %s\n", i + 1, __raxel_test_cases[i].name);
        __raxel_test_cases[i].func();
    }

    RAXEL_TEST_LOG("\nAll tests passed!\n");
    return 0;
}

/*
 * Macro: RAXEL_TEST_MAIN()
 * 
 * - Defines a main() function that calls raxel_run_all_tests().
 * - If you need a custom main(), skip this macro and call
 *   raxel_run_all_tests() manually.
 */
#define RAXEL_TEST_MAIN() \
    int main(void) { \
        return raxel_run_all_tests(); \
    }

#ifdef __cplusplus
}
#endif

#endif /* __RAXEL_TEST_H__ */
