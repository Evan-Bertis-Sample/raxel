#define RAXEL_APP_LOG_ENABLED

#include <stdio.h>
#include <raxel/core/util.h>
#include <raxel/core/graphics.h>
#include "tests/container.h"

int main() {
    register_container_tests();

    return raxel_test_run_all();
}