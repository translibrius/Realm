#include "../harness/rl_test.h"

#include "gui_internal.h"

RL_TEST(gui_id_never_returns_zero) {
    for (i32 i = 0; i < 1000; i++) {
        u32 id = gui__next_id();
        RL_EXPECT_MSG(id != 0, "gui__next_id() returned 0 at iteration %d", i);
    }
}

RL_TEST(gui_id_no_duplicates_first_1000) {
    u32 ids[1000];
    for (i32 i = 0; i < 1000; i++) {
        ids[i] = gui__next_id();
    }

    for (i32 i = 0; i < 1000; i++) {
        for (i32 j = i + 1; j < 1000; j++) {
            RL_EXPECT_MSG(ids[i] != ids[j],
                          "duplicate ID %u at indices %d and %d", ids[i], i, j);
        }
    }
}

RL_TEST(gui_id_hash_has_good_spread) {
    // The Jenkins hash should spread consecutive counter values widely
    // across the u32 range, not cluster them together.
    u32 a = gui__next_id();
    u32 b = gui__next_id();
    RL_EXPECT_MSG(a != b, "consecutive IDs should differ: a=%u b=%u", a, b);

    u32 diff = a > b ? a - b : b - a;
    RL_EXPECT_MSG(diff > 1000, "IDs should be well-separated (hash spread), diff=%u", diff);
}

void register_gui_id_tests(void) {
    rl_test_begin_group("gui_id");
    RL_REGISTER_TEST(gui_id_never_returns_zero);
    RL_REGISTER_TEST(gui_id_no_duplicates_first_1000);
    RL_REGISTER_TEST(gui_id_hash_has_good_spread);
}
