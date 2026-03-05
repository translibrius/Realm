#include "../harness/rl_test.h"

#include "platform/input.h"

RL_TEST(input_key_press_and_release_edges) {
    input_system_init();

    // Frame 0: nothing pressed
    input_update();
    RL_EXPECT(!input_is_key_down(KEY_A));
    RL_EXPECT(!input_key_pressed(KEY_A));
    RL_EXPECT(!input_key_released(KEY_A));

    // Frame 1: press A
    input_process_key(KEY_A, true);
    RL_EXPECT(input_is_key_down(KEY_A));
    RL_EXPECT(input_key_pressed(KEY_A));    // !prev && now
    RL_EXPECT(!input_key_released(KEY_A));

    // Frame 2: A still held — no longer "just pressed"
    input_update();
    RL_EXPECT(input_is_key_down(KEY_A));
    RL_EXPECT(!input_key_pressed(KEY_A));   // prev && now → not an edge
    RL_EXPECT(!input_key_released(KEY_A));

    // Frame 3: release A — edge exists between process and update
    input_process_key(KEY_A, false);
    RL_EXPECT(!input_is_key_down(KEY_A));
    RL_EXPECT(!input_key_pressed(KEY_A));
    RL_EXPECT(input_key_released(KEY_A));   // prev(true) && !now(false)

    // After update, the edge is gone
    input_update();
    RL_EXPECT(!input_key_released(KEY_A));
}

RL_TEST(input_mouse_button_edges) {
    input_system_init();
    input_update();

    // Press left mouse
    input_process_mouse_button(MOUSE_LEFT, true);
    RL_EXPECT(input_is_mouse_down(MOUSE_LEFT));
    RL_EXPECT(input_mouse_pressed(MOUSE_LEFT));
    RL_EXPECT(!input_mouse_released(MOUSE_LEFT));

    // Next frame — still held
    input_update();
    RL_EXPECT(input_is_mouse_down(MOUSE_LEFT));
    RL_EXPECT(!input_mouse_pressed(MOUSE_LEFT));

    // Release — edge exists between process and update
    input_process_mouse_button(MOUSE_LEFT, false);
    RL_EXPECT(!input_is_mouse_down(MOUSE_LEFT));
    RL_EXPECT(input_mouse_released(MOUSE_LEFT));

    // After update, edge is gone
    input_update();
    RL_EXPECT(!input_mouse_released(MOUSE_LEFT));
}

RL_TEST(input_mouse_move_tracks_position) {
    input_system_init();
    input_update();

    input_process_mouse_move(100, 200);

    vec2 pos;
    input_get_mouse_position(pos);
    RL_EXPECT_MSG((i32)pos[0] == 100, "x=%d expected=100", (i32)pos[0]);
    RL_EXPECT_MSG((i32)pos[1] == 200, "y=%d expected=200", (i32)pos[1]);
}

RL_TEST(input_mouse_delta_accumulates) {
    input_system_init();

    // Frame 0: establish baseline position
    input_process_mouse_move(0, 0);
    input_update();

    // Frame 1: two moves in one frame
    input_process_mouse_move(10, 5);
    input_process_mouse_move(25, 15);

    // Delta should be the accumulated movement within this frame
    // After update, prev gets the accumulated deltas
    input_update();

    vec2 delta;
    input_get_mouse_delta(delta);
    RL_EXPECT_MSG((i32)delta[0] == 25, "dx=%d expected=25", (i32)delta[0]);
    RL_EXPECT_MSG((i32)delta[1] == 15, "dy=%d expected=15", (i32)delta[1]);
}

RL_TEST(input_flush_mouse_delta_zeros_state) {
    input_system_init();

    input_process_mouse_move(0, 0);
    input_update();
    input_process_mouse_move(50, 30);
    input_update();

    // Flush should zero everything
    input_flush_mouse_delta();

    vec2 delta;
    input_get_mouse_delta(delta);
    RL_EXPECT_MSG((i32)delta[0] == 0, "dx=%d expected=0 after flush", (i32)delta[0]);
    RL_EXPECT_MSG((i32)delta[1] == 0, "dy=%d expected=0 after flush", (i32)delta[1]);

    // After another update, deltas should still be zero (flush_delta_frames > 0)
    input_update();
    input_get_mouse_delta(delta);
    RL_EXPECT_MSG((i32)delta[0] == 0, "dx=%d expected=0 post-flush frame", (i32)delta[0]);
    RL_EXPECT_MSG((i32)delta[1] == 0, "dy=%d expected=0 post-flush frame", (i32)delta[1]);
}

RL_TEST(input_update_copies_now_to_prev) {
    input_system_init();
    input_update();

    input_process_mouse_move(42, 84);
    input_update();

    vec2 prev;
    input_get_previous_mouse_position(prev);
    RL_EXPECT_MSG((i32)prev[0] == 42, "prev_x=%d expected=42", (i32)prev[0]);
    RL_EXPECT_MSG((i32)prev[1] == 84, "prev_y=%d expected=84", (i32)prev[1]);
}

void register_input_tests(void) {
    RL_REGISTER_TEST(input_key_press_and_release_edges);
    RL_REGISTER_TEST(input_mouse_button_edges);
    RL_REGISTER_TEST(input_mouse_move_tracks_position);
    RL_REGISTER_TEST(input_mouse_delta_accumulates);
    RL_REGISTER_TEST(input_flush_mouse_delta_zeros_state);
    RL_REGISTER_TEST(input_update_copies_now_to_prev);
}
