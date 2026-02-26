#include "../harness/rl_test.h"

#include "memory/arena.h"
#include "memory/memory.h"

#include <string.h>

// --- rl_arena_create / rl_arena_destroy (internal metadata) ---

RL_TEST(arena_create_returns_valid_pointer) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);
    RL_EXPECT(arena != nullptr);
    RL_EXPECT_MSG(arena->reserve_size >= KiB(64),
                  "reserve_size=%llu expected>=%llu", arena->reserve_size, KiB(64));
    RL_EXPECT_MSG(arena->pos == sizeof(rl_arena),
                  "pos=%llu expected=%llu", arena->pos, (u64)sizeof(rl_arena));
    rl_arena_destroy(arena);
}

// --- rl_arena_init / rl_arena_deinit (external metadata) ---

RL_TEST(arena_init_sets_fields_correctly) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    RL_EXPECT(arena.base != nullptr);
    RL_EXPECT_MSG(arena.reserve_size >= KiB(64),
                  "reserve_size=%llu expected>=%llu", arena.reserve_size, KiB(64));
    RL_EXPECT_MSG(arena.pos == 0, "pos=%llu expected=0", arena.pos);
    RL_EXPECT_EQ_I32(arena.mem_type, MEM_ARENA);

    rl_arena_deinit(&arena);
}

// --- Push tests (use rl_arena_create to exercise the primary path) ---

RL_TEST(arena_push_returns_aligned_pointer) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    // Push odd size — result should still be 8-byte aligned
    void *p1 = rl_arena_push(arena, 7, false);
    RL_EXPECT(p1 != nullptr);
    RL_EXPECT_MSG(((u64)p1 % 8) == 0,
                  "ptr=%p not 8-byte aligned", p1);

    void *p2 = rl_arena_push(arena, 13, false);
    RL_EXPECT(p2 != nullptr);
    RL_EXPECT_MSG(((u64)p2 % 8) == 0,
                  "ptr=%p not 8-byte aligned", p2);

    rl_arena_destroy(arena);
}

RL_TEST(arena_push_zero_clears_memory) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    u8 *ptr = rl_arena_push(arena, 64, true);
    RL_EXPECT(ptr != nullptr);

    for (u32 i = 0; i < 64; i++) {
        RL_EXPECT_MSG(ptr[i] == 0, "byte[%u] should be 0, got=%u", i, ptr[i]);
    }

    rl_arena_destroy(arena);
}

RL_TEST(arena_push_sequential_no_overlap) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    u8 *a = rl_arena_push(arena, 32, false);
    u8 *b = rl_arena_push(arena, 32, false);

    RL_EXPECT(a != nullptr);
    RL_EXPECT(b != nullptr);

    // Fill with distinct patterns
    memset(a, 0xAA, 32);
    memset(b, 0xBB, 32);

    // Verify first allocation was not overwritten
    for (u32 i = 0; i < 32; i++) {
        RL_EXPECT_MSG(a[i] == 0xAA, "a[%u] expected=0xAA got=0x%02X", i, a[i]);
    }
    for (u32 i = 0; i < 32; i++) {
        RL_EXPECT_MSG(b[i] == 0xBB, "b[%u] expected=0xBB got=0x%02X", i, b[i]);
    }

    // No overlap: b should start at or after a+32
    RL_EXPECT_MSG(b >= a + 32, "allocations overlap: a=%p b=%p", a, b);

    rl_arena_destroy(arena);
}

RL_TEST(arena_push_aligned_custom_alignment) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    // Push 1 byte to potentially misalign
    rl_arena_push(arena, 1, false);

    void *p64 = rl_arena_push_aligned(arena, 64, 64, false);
    RL_EXPECT(p64 != nullptr);
    RL_EXPECT_MSG(((u64)p64 % 64) == 0,
                  "ptr=%p not 64-byte aligned", p64);

    void *p16 = rl_arena_push_aligned(arena, 16, 16, false);
    RL_EXPECT(p16 != nullptr);
    RL_EXPECT_MSG(((u64)p16 % 16) == 0,
                  "ptr=%p not 16-byte aligned", p16);

    rl_arena_destroy(arena);
}

// --- Pop / Clear / Temp (use rl_arena_create — pop/clear clamp to ARENA_BASE_POS
//     which is correct for internal-metadata arenas) ---

RL_TEST(arena_pop_reduces_position) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    u64 pos_before = arena->pos;
    rl_arena_push(arena, 128, false);
    RL_EXPECT_MSG(arena->pos > pos_before,
                  "pos should have increased: before=%llu after=%llu", pos_before, arena->pos);

    rl_arena_pop(arena, 128);
    // After pop, pos should be back at (or very near) the initial base position
    RL_EXPECT_MSG(arena->pos <= pos_before + 8,
                  "pos after pop=%llu expected<=%llu", arena->pos, pos_before + 8);

    rl_arena_destroy(arena);
}

RL_TEST(arena_clear_resets_to_base) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    u64 base_pos = arena->pos; // sizeof(rl_arena) for created arenas

    // Push several allocations
    rl_arena_push(arena, 100, false);
    rl_arena_push(arena, 200, false);
    rl_arena_push(arena, 300, false);

    RL_EXPECT_MSG(arena->pos > base_pos,
                  "pos should be well past base after allocations");

    rl_arena_clear(arena);
    RL_EXPECT_MSG(arena->pos == base_pos,
                  "pos after clear=%llu expected=%llu", arena->pos, base_pos);

    rl_arena_destroy(arena);
}

RL_TEST(arena_temp_begin_end_restores_position) {
    rl_arena *arena = rl_arena_create(KiB(64), KiB(4), MEM_ARENA);

    // Push some initial data
    rl_arena_push(arena, 64, false);
    u64 saved_pos = arena->pos;

    // Begin temp scope
    rl_temp_arena temp = rl_arena_temp_begin(arena);

    // Push more inside temp scope
    rl_arena_push(arena, 256, false);
    RL_EXPECT_MSG(arena->pos > saved_pos,
                  "pos should advance in temp scope");

    // End temp scope — should restore
    rl_arena_temp_end(temp);
    RL_EXPECT_MSG(arena->pos == saved_pos,
                  "pos after temp_end=%llu expected=%llu", arena->pos, saved_pos);

    rl_arena_destroy(arena);
}

// --- Init-style arena: push past initial commit ---

RL_TEST(arena_init_push_past_initial_commit) {
    rl_arena arena = {0};
    // Small initial commit (4KiB) with large reserve — forces commit growth on push
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    // Push enough to exceed the initial 4KiB commit
    u8 *ptr = rl_arena_push(&arena, KiB(8), true);
    RL_EXPECT(ptr != nullptr);

    // Verify the memory is usable (zeroed and writable)
    for (u32 i = 0; i < KiB(8); i++) {
        RL_EXPECT_MSG(ptr[i] == 0, "byte[%u] should be 0, got=%u", i, ptr[i]);
    }

    // Write a pattern and read it back to confirm the committed memory is real
    memset(ptr, 0xCD, KiB(8));
    for (u32 i = 0; i < KiB(8); i++) {
        RL_EXPECT_MSG(ptr[i] == 0xCD, "byte[%u] expected=0xCD got=0x%02X", i, ptr[i]);
    }

    rl_arena_deinit(&arena);
}

// --- Init-style arena: pop and clear ---

RL_TEST(arena_init_pop_resets_to_zero) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    RL_EXPECT_MSG(arena.pos == 0, "init arena base pos=%llu expected=0", arena.pos);

    rl_arena_push(&arena, 256, false);
    RL_EXPECT_MSG(arena.pos > 0, "pos should advance after push");

    rl_arena_pop(&arena, 256);
    RL_EXPECT_MSG(arena.pos == 0,
                  "pos after pop=%llu expected=0", arena.pos);

    rl_arena_deinit(&arena);
}

RL_TEST(arena_init_clear_resets_to_zero) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_arena_push(&arena, 100, false);
    rl_arena_push(&arena, 200, false);
    rl_arena_push(&arena, 300, false);

    RL_EXPECT_MSG(arena.pos > 0, "pos should be past zero after pushes");

    rl_arena_clear(&arena);
    RL_EXPECT_MSG(arena.pos == 0,
                  "pos after clear=%llu expected=0", arena.pos);

    rl_arena_deinit(&arena);
}

RL_TEST(arena_init_temp_begin_end_restores_position) {
    rl_arena arena = {0};
    rl_arena_init(&arena, KiB(64), KiB(4), MEM_ARENA);

    rl_arena_push(&arena, 64, false);
    u64 saved_pos = arena.pos;

    rl_temp_arena temp = rl_arena_temp_begin(&arena);

    rl_arena_push(&arena, 256, false);
    RL_EXPECT_MSG(arena.pos > saved_pos, "pos should advance in temp scope");

    rl_arena_temp_end(temp);
    RL_EXPECT_MSG(arena.pos == saved_pos,
                  "pos after temp_end=%llu expected=%llu", arena.pos, saved_pos);

    rl_arena_deinit(&arena);
}

// --- Scratch arena (uses rl_arena_create internally) ---

RL_TEST(arena_scratch_get_returns_usable_memory) {
    rl_temp_arena scratch = rl_arena_scratch_get();
    RL_EXPECT(scratch.arena != nullptr);

    u8 *ptr = rl_arena_push(scratch.arena, 128, true);
    RL_EXPECT(ptr != nullptr);

    // Verify zeroed
    for (u32 i = 0; i < 128; i++) {
        RL_EXPECT_MSG(ptr[i] == 0, "scratch byte[%u] should be 0", i);
    }

    arena_scratch_release(scratch);
}

void register_arena_tests(void) {
    RL_REGISTER_TEST(arena_create_returns_valid_pointer);
    RL_REGISTER_TEST(arena_init_sets_fields_correctly);
    RL_REGISTER_TEST(arena_push_returns_aligned_pointer);
    RL_REGISTER_TEST(arena_push_zero_clears_memory);
    RL_REGISTER_TEST(arena_push_sequential_no_overlap);
    RL_REGISTER_TEST(arena_push_aligned_custom_alignment);
    RL_REGISTER_TEST(arena_pop_reduces_position);
    RL_REGISTER_TEST(arena_clear_resets_to_base);
    RL_REGISTER_TEST(arena_temp_begin_end_restores_position);
    RL_REGISTER_TEST(arena_init_push_past_initial_commit);
    RL_REGISTER_TEST(arena_init_pop_resets_to_zero);
    RL_REGISTER_TEST(arena_init_clear_resets_to_zero);
    RL_REGISTER_TEST(arena_init_temp_begin_end_restores_position);
    RL_REGISTER_TEST(arena_scratch_get_returns_usable_memory);
}
