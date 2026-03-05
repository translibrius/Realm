#include "../harness/rl_test.h"

// Re-implementation of profiler hash functions from engine/src/profiler/profiler.c
// to verify distribution and correctness properties.

#define AGG_MAP_SIZE  4096
#define EDGE_MAP_SIZE 4096

static u32 hash_ptr(void *ptr) {
    return (u32)((u64)ptr * 2654435761ULL) & (AGG_MAP_SIZE - 1);
}

static u32 hash_edge(void *parent, void *child) {
    u64 combined = (u64)parent * 2654435761ULL ^ (u64)child * 2246822519ULL;
    return (u32)combined & (EDGE_MAP_SIZE - 1);
}

RL_TEST(profiler_hash_ptr_distribution) {
    // Sequential function-like addresses (16-byte aligned) should spread across buckets
    u32 buckets[16] = {0};
    for (u64 i = 0; i < 256; i++) {
        void *addr = (void *)(0x100000 + i * 16);
        u32 slot = hash_ptr(addr);
        buckets[slot / (AGG_MAP_SIZE / 16)]++;
    }

    // Each of 16 bins should have some entries (no severe clustering)
    for (i32 i = 0; i < 16; i++) {
        RL_EXPECT_MSG(buckets[i] > 0,
                      "bucket %d has 0 entries — hash is clustering", i);
    }
}

RL_TEST(profiler_hash_edge_distinct_pairs) {
    // Different parent→child pairs should produce different hashes
    void *a = (void *)0x100000;
    void *b = (void *)0x200010;
    void *c = (void *)0x300020;

    u32 ab = hash_edge(a, b);
    u32 ac = hash_edge(a, c);
    u32 bc = hash_edge(b, c);

    RL_EXPECT_MSG(ab != ac, "hash_edge(a,b) == hash_edge(a,c): %u", ab);
    RL_EXPECT_MSG(ab != bc, "hash_edge(a,b) == hash_edge(b,c): %u", ab);
    RL_EXPECT_MSG(ac != bc, "hash_edge(a,c) == hash_edge(b,c): %u", ac);
}

RL_TEST(profiler_hash_ptr_deterministic) {
    void *addr = (void *)0xDEADBEEF;
    u32 h1 = hash_ptr(addr);
    u32 h2 = hash_ptr(addr);
    RL_EXPECT_EQ_U32(h1, h2);
}

RL_TEST(profiler_hash_ptr_in_range) {
    // All results must be within [0, AGG_MAP_SIZE)
    for (u64 i = 0; i < 1000; i++) {
        void *addr = (void *)(i * 7 + 0x400000);
        u32 slot = hash_ptr(addr);
        RL_EXPECT_MSG(slot < AGG_MAP_SIZE,
                      "hash_ptr(%p) = %u, out of range [0, %u)", addr, slot, AGG_MAP_SIZE);
    }
}

RL_TEST(profiler_hash_edge_in_range) {
    for (u64 i = 0; i < 100; i++) {
        void *p = (void *)(0x1000 + i * 16);
        void *c = (void *)(0x2000 + i * 32);
        u32 slot = hash_edge(p, c);
        RL_EXPECT_MSG(slot < EDGE_MAP_SIZE,
                      "hash_edge out of range: %u", slot);
    }
}

void register_profiler_tests(void) {
    rl_test_begin_group("profiler");
    RL_REGISTER_TEST(profiler_hash_ptr_distribution);
    RL_REGISTER_TEST(profiler_hash_edge_distinct_pairs);
    RL_REGISTER_TEST(profiler_hash_ptr_deterministic);
    RL_REGISTER_TEST(profiler_hash_ptr_in_range);
    RL_REGISTER_TEST(profiler_hash_edge_in_range);
}
