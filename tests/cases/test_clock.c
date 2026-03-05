#include "../harness/rl_test.h"

#include "util/clock.h"
#include "platform/platform.h"

// --- Tests ---

RL_TEST(clock_reset_sets_positive_frequency) {
    rl_clock c = {0};
    clock_reset(&c);

    RL_EXPECT_MSG(c.frequency > 0, "frequency=%lld", c.frequency);
    RL_EXPECT_MSG(c.start > 0, "start=%lld", c.start);
}

RL_TEST(clock_update_advances_last) {
    rl_clock c = {0};
    clock_reset(&c);
    clock_update(&c);

    RL_EXPECT_MSG(c.last >= c.start, "last=%lld start=%lld", c.last, c.start);
}

RL_TEST(clock_elapsed_non_negative) {
    rl_clock c = {0};
    clock_reset(&c);
    clock_update(&c);

    f64 elapsed = clock_elapsed_s(&c);
    RL_EXPECT_MSG(elapsed >= 0.0, "elapsed=%f", elapsed);
}

RL_TEST(clock_elapsed_after_sleep) {
    rl_clock c = {0};
    clock_reset(&c);
    platform_sleep(10);
    clock_update(&c);

    f64 elapsed = clock_elapsed_s(&c);
    RL_EXPECT_MSG(elapsed >= 0.005, "elapsed=%f expected>=0.005", elapsed);
}

void register_clock_tests(void) {
    rl_test_begin_group("clock");
    RL_REGISTER_TEST(clock_reset_sets_positive_frequency);
    RL_REGISTER_TEST(clock_update_advances_last);
    RL_REGISTER_TEST(clock_elapsed_non_negative);
    RL_REGISTER_TEST(clock_elapsed_after_sleep);
}
