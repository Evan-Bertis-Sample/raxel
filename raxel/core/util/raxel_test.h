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

#include "raxel_debug.h"

/*
 * Each test case has:
 *  - A test function (no arguments, no return).
 *  - A name for reporting.
 */
typedef void (*raxel_test_fn_t)(void);

typedef struct raxel_test_case_s {
    const char *name;
    raxel_test_fn_t func;
} raxel_test_case_t;

static raxel_test_case_t __raxel_test_cases[1024];
static size_t __raxel_test_case_count = 0;

#define RAXEL_TEST_LOG RAXEL_CORE_LOG
#define RAXEL_TEST_LOG_ERROR RAXEL_CORE_LOG_ERROR
#define RAXEL_TEST_FATAL RAXEL_CORE_LOG_ERROR

#define RAXEL_TEST(name)                                           \
    static void name(void);                                        \
    static void name(void)

static void raxel_test_register(const char *test_name, raxel_test_fn_t fn) {
    __raxel_test_cases[__raxel_test_case_count].name = test_name;
    __raxel_test_cases[__raxel_test_case_count].func = fn;
    __raxel_test_case_count++;
}

#define RAXEL_TEST_ASSERT(expr)                                       \
    do {                                                              \
        if (!(expr)) {                                                \
            RAXEL_TEST_FATAL("Assertion failed in test '%s': (%s)\n", \
                             __func__, #expr);                        \
        }                                                             \
    } while (0)

static int raxel_test_run_all(void) {
    RAXEL_TEST_LOG("Running %zu test(s)...\n", __raxel_test_case_count);

    for (size_t i = 0; i < __raxel_test_case_count; i++) {
        RAXEL_TEST_LOG("  Test #%zu: %s\n", i + 1, __raxel_test_cases[i].name);
        __raxel_test_cases[i].func();
    }

    RAXEL_TEST_LOG("\nAll tests passed!\n");
    return 0;
}

#define RAXEL_TEST_REGISTER(test_name) raxel_test_register(#test_name, test_name)


#ifdef __cplusplus
}
#endif

#endif /* __RAXEL_TEST_H__ */
