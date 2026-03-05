#include "../harness/rl_test.h"

#include "util/rand.h"

// --- Tests ---

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

RL_TEST(rand_float01_bounds) {
    for (i32 i = 0; i < 1000; i++) {
        f64 v = rand_float01();
        RL_EXPECT_MSG(v >= 0.0 && v <= 1.0, "iter=%d got=%f", i, v);
    }
}

void register_rand_tests(void) {
    RL_REGISTER_TEST(rand_int_range_single_value);
    RL_REGISTER_TEST(rand_int_range_bounds);
    RL_REGISTER_TEST(rand_int_range_negative);
    RL_REGISTER_TEST(rand_float01_bounds);
}
