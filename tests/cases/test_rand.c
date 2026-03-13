#include "../harness/rl_test.h"

#include "util/rand.h"

RL_TEST(rand_int_range_single_value) {
    for (i32 i = 0; i < 100; i++) {
        i64 v = rand_int_range(5, 5);
        RL_EXPECT_MSG(v == 5, "iter=%d got=%lld", i, v);
    }
}

RL_TEST(rand_int_range_bounds) {
    for (i32 i = 0; i < 1000; i++) {
        i64 v = rand_int_range(0, 10);
        RL_EXPECT_MSG(v >= 0 && v <= 10, "iter=%d got=%lld", i, v);
    }
}

RL_TEST(rand_int_range_negative) {
    for (i32 i = 0; i < 1000; i++) {
        i64 v = rand_int_range(-5, 5);
        RL_EXPECT_MSG(v >= -5 && v <= 5, "iter=%d got=%lld", i, v);
    }
}

RL_TEST(rand_int_range_covers_range) {
    // After 1000 draws from [0, 10], at least 8 of the 11 values should appear.
    // An implementation that always returns a constant would fail this.
    b8 seen[11] = {0};
    for (i32 i = 0; i < 1000; i++) {
        i64 v = rand_int_range(0, 10);
        if (v >= 0 && v <= 10) {
            seen[v] = true;
        }
    }

    i32 distinct = 0;
    for (i32 i = 0; i <= 10; i++) {
        if (seen[i]) distinct++;
    }
    RL_EXPECT_MSG(distinct >= 8, "expected >= 8 distinct values, got %d", distinct);
}

RL_TEST(rand_float01_bounds) {
    for (i32 i = 0; i < 1000; i++) {
        f64 v = rand_float01();
        RL_EXPECT_MSG(v >= 0.0 && v <= 1.0, "iter=%d got=%f", i, v);
    }
}

RL_TEST(rand_float01_has_spread) {
    // Verify the output actually spans most of [0, 1].
    // A degenerate implementation returning a constant would fail this.
    f64 lo = 1.0, hi = 0.0;
    for (i32 i = 0; i < 1000; i++) {
        f64 v = rand_float01();
        if (v < lo) lo = v;
        if (v > hi) hi = v;
    }
    RL_EXPECT_MSG(lo < 0.1, "min sample=%f, expected < 0.1", lo);
    RL_EXPECT_MSG(hi > 0.9, "max sample=%f, expected > 0.9", hi);
}

void register_rand_tests(void) {
    rl_test_begin_group("rand");
    RL_REGISTER_TEST(rand_int_range_single_value);
    RL_REGISTER_TEST(rand_int_range_bounds);
    RL_REGISTER_TEST(rand_int_range_negative);
    RL_REGISTER_TEST(rand_int_range_covers_range);
    RL_REGISTER_TEST(rand_float01_bounds);
    RL_REGISTER_TEST(rand_float01_has_spread);
}
