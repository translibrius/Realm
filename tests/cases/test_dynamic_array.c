#include "../harness/rl_test.h"

#include "memory/containers/dynamic_array.h"
#include "memory/memory.h"

DA_DEFINE(IntArray, i32);

RL_TEST(da_init_starts_empty) {
    IntArray arr;
    da_init(&arr);

    RL_EXPECT_MSG(arr.count == 0, "count=%llu expected=0", arr.count);
    RL_EXPECT_MSG(arr.capacity == 0, "capacity=%llu expected=0", arr.capacity);
    RL_EXPECT(arr.items == nullptr);
}

RL_TEST(da_init_with_cap_preallocates) {
    IntArray arr;
    da_init_with_cap(&arr, 20);

    RL_EXPECT_MSG(arr.count == 0, "count=%llu expected=0", arr.count);
    RL_EXPECT_MSG(arr.capacity == 20, "capacity=%llu expected=20", arr.capacity);
    RL_EXPECT(arr.items != nullptr);

    da_free(&arr);
}

RL_TEST(da_append_single_element) {
    IntArray arr;
    da_init(&arr);

    da_append(&arr, 42);

    RL_EXPECT_MSG(arr.count == 1, "count=%llu expected=1", arr.count);
    RL_EXPECT_EQ_I32(arr.items[0], 42);
    // First append from 0 capacity should grow to DEFAULT_CAPACITY (12)
    RL_EXPECT_MSG(arr.capacity == 12, "capacity=%llu expected=12", arr.capacity);

    da_free(&arr);
}

RL_TEST(da_append_triggers_growth) {
    IntArray arr;
    da_init_with_cap(&arr, 2);

    da_append(&arr, 10);
    da_append(&arr, 20);
    da_append(&arr, 30); // Should trigger growth past capacity 2

    RL_EXPECT_MSG(arr.count == 3, "count=%llu expected=3", arr.count);
    RL_EXPECT_MSG(arr.capacity > 2, "capacity=%llu should be >2", arr.capacity);
    RL_EXPECT_EQ_I32(arr.items[0], 10);
    RL_EXPECT_EQ_I32(arr.items[1], 20);
    RL_EXPECT_EQ_I32(arr.items[2], 30);

    da_free(&arr);
}

RL_TEST(da_append_many_items_all_correct) {
    IntArray arr;
    da_init(&arr);

    for (i32 i = 0; i < 200; i++) {
        da_append(&arr, i * 7);
    }

    RL_EXPECT_MSG(arr.count == 200, "count=%llu expected=200", arr.count);

    for (i32 i = 0; i < 200; i++) {
        RL_EXPECT_MSG(arr.items[i] == i * 7,
                      "items[%d] expected=%d got=%d", i, i * 7, arr.items[i]);
    }

    da_free(&arr);
}

RL_TEST(da_free_resets_all_fields) {
    IntArray arr;
    da_init(&arr);

    da_append(&arr, 1);
    da_append(&arr, 2);
    da_append(&arr, 3);

    da_free(&arr);

    RL_EXPECT_MSG(arr.count == 0, "count=%llu expected=0", arr.count);
    RL_EXPECT_MSG(arr.capacity == 0, "capacity=%llu expected=0", arr.capacity);
    RL_EXPECT(arr.items == nullptr);
}

void register_dynamic_array_tests(void) {
    RL_REGISTER_TEST(da_init_starts_empty);
    RL_REGISTER_TEST(da_init_with_cap_preallocates);
    RL_REGISTER_TEST(da_append_single_element);
    RL_REGISTER_TEST(da_append_triggers_growth);
    RL_REGISTER_TEST(da_append_many_items_all_correct);
    RL_REGISTER_TEST(da_free_resets_all_fields);
}
