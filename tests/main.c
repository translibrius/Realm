#include "harness/rl_test.h"
#include "support/test_runtime.h"

#include <stdio.h>

void register_camera_tests(void);
void register_str_tests(void);

int main(int argc, const char **argv) {
    if (!rl_test_runtime_init()) {
        fprintf(stderr, "Failed to initialize test runtime\n");
        return 1;
    }

    register_camera_tests();
    register_str_tests();

    i32 result = rl_test_run_from_args(argc, argv);

    rl_test_runtime_shutdown();
    return result;
}
