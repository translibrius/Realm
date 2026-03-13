#include "harness/rl_test.h"
#include "support/test_runtime.h"

#include <stdio.h>

void register_camera_tests(void);
void register_config_tests(void);
void register_str_tests(void);
void register_memory_tests(void);
void register_arena_tests(void);
void register_dynamic_array_tests(void);
void register_event_tests(void);
void register_input_tests(void);
void register_logger_tests(void);
void register_file_io_tests(void);
void register_clock_tests(void);
void register_rand_tests(void);
void register_gui_id_tests(void);
void register_text_input_tests(void);
void register_asset_tests(void);
void register_profiler_tests(void);
void register_entity_tests(void);

int main(int argc, const char **argv) {
    if (!rl_test_runtime_init()) {
        fprintf(stderr, "Failed to initialize test runtime\n");
        return 1;
    }

    register_camera_tests();
    register_config_tests();
    register_str_tests();
    register_memory_tests();
    register_arena_tests();
    register_dynamic_array_tests();
    register_event_tests();
    register_input_tests();
    register_logger_tests();
    register_file_io_tests();
    register_clock_tests();
    register_rand_tests();
    register_gui_id_tests();
    register_text_input_tests();
    register_asset_tests();
    register_profiler_tests();
    register_entity_tests();

    i32 result = rl_test_run_from_args(argc, argv);

    rl_test_runtime_shutdown();
    return result;
}
