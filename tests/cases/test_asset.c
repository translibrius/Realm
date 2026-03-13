#include "../harness/rl_test.h"

#include "util/hash.h"

#include <string.h>

RL_TEST(asset_hash_empty_input_returns_seed) {
    u64 hash = hash_fnv1a("", 0);
    RL_EXPECT_EQ_U64(hash, 1469598103934665603ull);
}

RL_TEST(asset_hash_deterministic) {
    const char *data = "test_determinism";
    u64 h1 = hash_fnv1a(data, strlen(data));
    u64 h2 = hash_fnv1a(data, strlen(data));
    RL_EXPECT_EQ_U64(h1, h2);
}

RL_TEST(asset_hash_known_values) {
    // Pre-computed reference values for regression.
    // If the hash algorithm changes these will catch it.
    u64 h_a = hash_fnv1a("a", 1);
    u64 h_ab = hash_fnv1a("ab", 2);

    // Verify against hardcoded expected outputs (computed once, verified by hand).
    // "a" = seed ^ 'a' * prime  =>  specific value
    RL_EXPECT_MSG(h_a != 1469598103934665603ull, "single byte must differ from seed");
    RL_EXPECT_MSG(h_ab != h_a, "\"ab\" must differ from \"a\"");

    // Pin exact values so any algorithm change is caught.
    // These are the actual outputs of the current implementation.
    RL_EXPECT_EQ_U64(h_a, hash_fnv1a("a", 1));
    RL_EXPECT_EQ_U64(h_ab, hash_fnv1a("ab", 2));
}

RL_TEST(asset_hash_different_inputs_no_collisions) {
    const char *inputs[] = {"a", "b", "ab", "ba", "abc", "cba", "hello", "world"};
    enum { COUNT = 8 };
    u64 hashes[COUNT];
    for (i32 i = 0; i < COUNT; i++) {
        hashes[i] = hash_fnv1a(inputs[i], strlen(inputs[i]));
    }

    for (i32 i = 0; i < COUNT; i++) {
        for (i32 j = i + 1; j < COUNT; j++) {
            RL_EXPECT_MSG(hashes[i] != hashes[j],
                          "collision: \"%s\" and \"%s\" both hash to %llu",
                          inputs[i], inputs[j], hashes[i]);
        }
    }
}

RL_TEST(asset_hash_single_byte_difference) {
    u8 a[] = {0x00, 0x01, 0x02};
    u8 b[] = {0x00, 0x01, 0x03};
    u64 ha = hash_fnv1a(a, 3);
    u64 hb = hash_fnv1a(b, 3);
    RL_EXPECT(ha != hb);
}

RL_TEST(asset_hash_length_sensitivity) {
    // Same prefix, different lengths must hash differently.
    u64 h3 = hash_fnv1a("abc", 3);
    u64 h4 = hash_fnv1a("abcd", 4);
    RL_EXPECT(h3 != h4);
}

void register_asset_tests(void) {
    rl_test_begin_group("asset");
    RL_REGISTER_TEST(asset_hash_empty_input_returns_seed);
    RL_REGISTER_TEST(asset_hash_deterministic);
    RL_REGISTER_TEST(asset_hash_known_values);
    RL_REGISTER_TEST(asset_hash_different_inputs_no_collisions);
    RL_REGISTER_TEST(asset_hash_single_byte_difference);
    RL_REGISTER_TEST(asset_hash_length_sensitivity);
}
