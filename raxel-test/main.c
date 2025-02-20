#define RAXEL_APP_LOG_ENABLED

#include <stdio.h>
#include <raxel/core/util.h>
#include "tests/container.h"
#include "tests/hashtable.h"
#include "tests/bvh.h"

int main() {
    register_container_tests();
    register_hashtable_tests();
    register_bvh_tests();
    return raxel_test_run_all();
}