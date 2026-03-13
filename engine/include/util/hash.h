#pragma once

#include "defines.h"

// FNV-1a variant (64-bit) used for asset source hashing.
static inline u64 hash_fnv1a(const void *data, u64 length) {
    const u8 *bytes = (const u8 *)data;
    u64 hash = 1469598103934665603ull;
    for (u64 i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    return hash;
}
