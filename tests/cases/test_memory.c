#include "../harness/rl_test.h"

#include "memory/memory.h"

#include <string.h>

RL_TEST(memory_alloc_returns_nonnull) {
    void *ptr = mem_alloc(128, MEM_UNKNOWN);
    RL_EXPECT(ptr != nullptr);
    mem_free(ptr, 128, MEM_UNKNOWN);
}

RL_TEST(memory_alloc_different_types) {
    void *a = mem_alloc(64, MEM_APPLICATION);
    void *b = mem_alloc(64, MEM_STRING);
    void *c = mem_alloc(64, MEM_DYNAMIC_ARRAY);

    RL_EXPECT(a != nullptr);
    RL_EXPECT(b != nullptr);
    RL_EXPECT(c != nullptr);

    mem_free(a, 64, MEM_APPLICATION);
    mem_free(b, 64, MEM_STRING);
    mem_free(c, 64, MEM_DYNAMIC_ARRAY);
}

RL_TEST(memory_realloc_preserves_data) {
    u8 *ptr = mem_alloc(64, MEM_UNKNOWN);
    RL_EXPECT(ptr != nullptr);

    for (u32 i = 0; i < 64; i++) {
        ptr[i] = (u8)(i + 1);
    }

    ptr = mem_realloc(ptr, 64, 256, MEM_UNKNOWN);
    RL_EXPECT(ptr != nullptr);

    for (u32 i = 0; i < 64; i++) {
        RL_EXPECT_MSG(ptr[i] == (u8)(i + 1),
                      "byte[%u] expected=%u got=%u", i, (u8)(i + 1), ptr[i]);
    }

    mem_free(ptr, 256, MEM_UNKNOWN);
}

RL_TEST(memory_realloc_grows_allocation) {
    u8 *ptr = mem_alloc(32, MEM_UNKNOWN);
    RL_EXPECT(ptr != nullptr);

    ptr = mem_realloc(ptr, 32, 1024, MEM_UNKNOWN);
    RL_EXPECT(ptr != nullptr);

    // Write to the end of the grown allocation
    ptr[1023] = 0xAB;
    RL_EXPECT(ptr[1023] == 0xAB);

    mem_free(ptr, 1024, MEM_UNKNOWN);
}

RL_TEST(memory_zero_clears_memory) {
    u8 *ptr = mem_alloc(64, MEM_UNKNOWN);
    RL_EXPECT(ptr != nullptr);

    memset(ptr, 0xFF, 64);
    mem_zero(ptr, 64);

    for (u32 i = 0; i < 64; i++) {
        RL_EXPECT_MSG(ptr[i] == 0, "byte[%u] should be 0, got=%u", i, ptr[i]);
    }

    mem_free(ptr, 64, MEM_UNKNOWN);
}

RL_TEST(memory_copy_copies_data_correctly) {
    u8 *src = mem_alloc(64, MEM_UNKNOWN);
    u8 *dst = mem_alloc(64, MEM_UNKNOWN);
    RL_EXPECT(src != nullptr);
    RL_EXPECT(dst != nullptr);

    for (u32 i = 0; i < 64; i++) {
        src[i] = (u8)(i * 3);
    }
    mem_zero(dst, 64);

    // mem_copy(origin, destination, size) - origin is source
    mem_copy(src, dst, 64);

    for (u32 i = 0; i < 64; i++) {
        RL_EXPECT_MSG(dst[i] == src[i],
                      "byte[%u] expected=%u got=%u", i, src[i], dst[i]);
    }

    mem_free(src, 64, MEM_UNKNOWN);
    mem_free(dst, 64, MEM_UNKNOWN);
}

RL_TEST(memory_free_and_realloc_cycle) {
    // 10 alloc/free cycles
    for (u32 i = 0; i < 10; i++) {
        void *ptr = mem_alloc(64 + i * 16, MEM_UNKNOWN);
        RL_EXPECT(ptr != nullptr);
        mem_free(ptr, 64 + i * 16, MEM_UNKNOWN);
    }

    // 10 alloc/realloc/free cycles
    for (u32 i = 0; i < 10; i++) {
        u64 initial = 32 + i * 8;
        u64 grown = initial * 4;
        void *ptr = mem_alloc(initial, MEM_APPLICATION);
        RL_EXPECT(ptr != nullptr);
        ptr = mem_realloc(ptr, initial, grown, MEM_APPLICATION);
        RL_EXPECT(ptr != nullptr);
        mem_free(ptr, grown, MEM_APPLICATION);
    }
}

RL_TEST(memory_type_to_str_returns_valid_strings) {
    for (i32 i = 0; i <= (i32)MEM_TYPES_MAX; i++) {
        const char *str = mem_type_to_str((MEM_TYPE)i);
        RL_EXPECT_MSG(str != nullptr, "mem_type_to_str(%d) returned null", i);
        RL_EXPECT_MSG(str[0] != '\0', "mem_type_to_str(%d) returned empty", i);
    }
}

void register_memory_tests(void) {
    rl_test_begin_group("memory");
    RL_REGISTER_TEST(memory_alloc_returns_nonnull);
    RL_REGISTER_TEST(memory_alloc_different_types);
    RL_REGISTER_TEST(memory_realloc_preserves_data);
    RL_REGISTER_TEST(memory_realloc_grows_allocation);
    RL_REGISTER_TEST(memory_zero_clears_memory);
    RL_REGISTER_TEST(memory_copy_copies_data_correctly);
    RL_REGISTER_TEST(memory_free_and_realloc_cycle);
    RL_REGISTER_TEST(memory_type_to_str_returns_valid_strings);
}
