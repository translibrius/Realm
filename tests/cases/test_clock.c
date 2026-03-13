#include "../harness/rl_test.h"

#include "util/clock.h"
#include "platform/platform.h"

RL_TEST(clock_reset_populates_fields) {
    rl_clock c = {0};
    clock_reset(&c);

    RL_EXPECT_MSG(c.frequency > 0, "frequency=%lld", c.frequency);
    RL_EXPECT_MSG(c.start > 0, "start=%lld", c.start);
}

RL_TEST(clock_update_advances_last_past_start) {
    rl_clock c = {0};
    clock_reset(&c);

    // Burn a tiny bit of time so the counter actually moves.
    volatile i32 dummy = 0;
    for (i32 i = 0; i < 10000; i++) dummy += i;
    (void)dummy;

    clock_update(&c);
    RL_EXPECT_MSG(c.last >= c.start, "last=%lld start=%lld", c.last, c.start);
}

RL_TEST(clock_elapsed_increases_after_work) {
    rl_clock c = {0};
    clock_reset(&c);

    // Do some real work so elapsed > 0.
    volatile i32 dummy = 0;
    for (i32 i = 0; i < 100000; i++) dummy += i;
    (void)dummy;

    clock_update(&c);
    f64 elapsed = clock_elapsed_s(&c);
    RL_EXPECT_MSG(elapsed > 0.0, "elapsed=%f, expected > 0 after work", elapsed);
}

RL_TEST(clock_reset_resets_elapsed) {
    rl_clock c = {0};
    clock_reset(&c);

    // Accumulate some time.
    platform_sleep(5);
    clock_update(&c);
    f64 before = clock_elapsed_s(&c);
    RL_EXPECT_MSG(before > 0.0, "before=%f", before);

    // Reset should bring elapsed back near zero.
    clock_reset(&c);
    clock_update(&c);
    f64 after = clock_elapsed_s(&c);
    RL_EXPECT_MSG(after < before, "after reset elapsed=%f should be less than before=%f", after, before);
}

void register_clock_tests(void) {
    rl_test_begin_group("clock");
    RL_REGISTER_TEST(clock_reset_populates_fields);
    RL_REGISTER_TEST(clock_update_advances_last_past_start);
    RL_REGISTER_TEST(clock_elapsed_increases_after_work);
    RL_REGISTER_TEST(clock_reset_resets_elapsed);
}
