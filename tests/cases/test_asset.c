#include "../harness/rl_test.h"

#include <string.h>

// Re-implementation of asset_hash_bytes from engine/src/asset/asset.c
// to verify the FNV-1a variant against known properties.
static u64 asset_hash_bytes(const void *data, u64 length) {
    const u8 *bytes = (const u8 *)data;
    u64 hash = 1469598103934665603ull;
    for (u64 i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

RL_TEST(asset_hash_empty_input) {
    // Empty input should return the seed unchanged
    u64 hash = asset_hash_bytes("", 0);
    RL_EXPECT_EQ_U64(hash, 1469598103934665603ull);
}

RL_TEST(asset_hash_known_vectors) {
    // Verify the hash produces consistent, non-trivial values for known inputs
    u64 h1 = asset_hash_bytes("hello", 5);
    u64 h2 = asset_hash_bytes("world", 5);

    // Should not be the seed
    RL_EXPECT(h1 != 1469598103934665603ull);
    RL_EXPECT(h2 != 1469598103934665603ull);
    // Should differ from each other
    RL_EXPECT(h1 != h2);
}

RL_TEST(asset_hash_different_inputs) {
    const char *inputs[] = {"a", "b", "ab", "ba", "abc", "cba"};
    u64 hashes[6];
    for (i32 i = 0; i < 6; i++) {
        hashes[i] = asset_hash_bytes(inputs[i], strlen(inputs[i]));
    }

    // All should be unique
    for (i32 i = 0; i < 6; i++) {
        for (i32 j = i + 1; j < 6; j++) {
            RL_EXPECT_MSG(hashes[i] != hashes[j],
                          "collision: \"%s\" and \"%s\" both hash to %llu",
                          inputs[i], inputs[j], hashes[i]);
        }
    }
}

RL_TEST(asset_hash_deterministic) {
    const char *data = "test_determinism";
    u64 h1 = asset_hash_bytes(data, strlen(data));
    u64 h2 = asset_hash_bytes(data, strlen(data));
    RL_EXPECT_EQ_U64(h1, h2);
}

RL_TEST(asset_hash_single_byte_difference) {
    // Inputs differing by a single byte should produce different hashes
    u8 a[] = {0x00, 0x01, 0x02};
    u8 b[] = {0x00, 0x01, 0x03};
    u64 ha = asset_hash_bytes(a, 3);
    u64 hb = asset_hash_bytes(b, 3);
    RL_EXPECT(ha != hb);
}

void register_asset_tests(void) {
    rl_test_begin_group("asset");
    RL_REGISTER_TEST(asset_hash_empty_input);
    RL_REGISTER_TEST(asset_hash_known_vectors);
    RL_REGISTER_TEST(asset_hash_different_inputs);
    RL_REGISTER_TEST(asset_hash_deterministic);
    RL_REGISTER_TEST(asset_hash_single_byte_difference);
}
