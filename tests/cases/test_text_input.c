#include "../harness/rl_test.h"

#include "gui/gui_text_input.h"
#include "platform/input.h"

#include <string.h>

static void type_char(gui_text_input_state *s, char c) {
    input_char ch = {.codepoint = (u32)c};
    gui_text_input_handle_char(s, &ch);
}

static b8 press_key(gui_text_input_state *s, KEYBOARD_KEY k) {
    input_key key = {.key = k, .pressed = true};
    return gui_text_input_handle_key(s, &key);
}

RL_TEST(text_input_char_insertion) {
    gui_text_input_state s = {0};
    type_char(&s, 'a');
    type_char(&s, 'b');
    type_char(&s, 'c');

    RL_EXPECT_STR_EQ(s.buf, "abc");
    RL_EXPECT_EQ_U32(s.len, 3);
    RL_EXPECT_EQ_U32(s.cursor, 3);
}

RL_TEST(text_input_backspace) {
    gui_text_input_state s = {0};
    type_char(&s, 'a');
    type_char(&s, 'b');
    type_char(&s, 'c');
    press_key(&s, KEY_BACKSPACE);

    RL_EXPECT_STR_EQ(s.buf, "ab");
    RL_EXPECT_EQ_U32(s.len, 2);
    RL_EXPECT_EQ_U32(s.cursor, 2);
}

RL_TEST(text_input_cursor_movement) {
    gui_text_input_state s = {0};
    type_char(&s, 'a');
    type_char(&s, 'b');
    type_char(&s, 'c');

    press_key(&s, KEY_LEFT);
    RL_EXPECT_EQ_U32(s.cursor, 2);
    press_key(&s, KEY_LEFT);
    RL_EXPECT_EQ_U32(s.cursor, 1);
    press_key(&s, KEY_RIGHT);
    RL_EXPECT_EQ_U32(s.cursor, 2);
}

RL_TEST(text_input_cursor_bounds) {
    gui_text_input_state s = {0};
    // Left at 0 stays 0
    press_key(&s, KEY_LEFT);
    RL_EXPECT_EQ_U32(s.cursor, 0);

    type_char(&s, 'x');
    // Right at end stays at end
    press_key(&s, KEY_RIGHT);
    RL_EXPECT_EQ_U32(s.cursor, 1);
}

RL_TEST(text_input_enter_returns_true) {
    gui_text_input_state s = {0};
    type_char(&s, 'h');
    type_char(&s, 'i');

    b8 submitted = press_key(&s, KEY_ENTER);
    RL_EXPECT(submitted);
}

RL_TEST(text_input_max_length) {
    gui_text_input_state s = {0};
    for (i32 i = 0; i < GUI_TEXT_INPUT_MAX; i++) {
        type_char(&s, 'a');
    }
    // Should cap at GUI_TEXT_INPUT_MAX - 1
    RL_EXPECT_EQ_U32(s.len, GUI_TEXT_INPUT_MAX - 1);

    // One more should be rejected
    type_char(&s, 'z');
    RL_EXPECT_EQ_U32(s.len, GUI_TEXT_INPUT_MAX - 1);
}

RL_TEST(text_input_insert_at_middle) {
    gui_text_input_state s = {0};
    type_char(&s, 'a');
    type_char(&s, 'c');
    press_key(&s, KEY_LEFT); // cursor at 1
    type_char(&s, 'b');      // insert 'b' at position 1

    RL_EXPECT_STR_EQ(s.buf, "abc");
    RL_EXPECT_EQ_U32(s.cursor, 2);
}

RL_TEST(text_input_non_printable_ignored) {
    gui_text_input_state s = {0};
    input_char ch_low = {.codepoint = 10}; // newline
    gui_text_input_handle_char(&s, &ch_low);
    RL_EXPECT_EQ_U32(s.len, 0);

    input_char ch_high = {.codepoint = 200}; // above printable
    gui_text_input_handle_char(&s, &ch_high);
    RL_EXPECT_EQ_U32(s.len, 0);
}

RL_TEST(text_input_display_caret) {
    gui_text_input_state s = {0};
    type_char(&s, 'a');
    type_char(&s, 'b');

    char out[64];
    // dt=0 means cursor_blink stays at 0 which is < 0.5, so caret is visible
    gui_text_input_display(&s, 0.0f, out, sizeof(out));
    // Caret should appear as '|' at cursor position (end)
    RL_EXPECT_STR_EQ(out, "ab|");
}

void register_text_input_tests(void) {
    rl_test_begin_group("text_input");
    RL_REGISTER_TEST(text_input_char_insertion);
    RL_REGISTER_TEST(text_input_backspace);
    RL_REGISTER_TEST(text_input_cursor_movement);
    RL_REGISTER_TEST(text_input_cursor_bounds);
    RL_REGISTER_TEST(text_input_enter_returns_true);
    RL_REGISTER_TEST(text_input_max_length);
    RL_REGISTER_TEST(text_input_insert_at_middle);
    RL_REGISTER_TEST(text_input_non_printable_ignored);
    RL_REGISTER_TEST(text_input_display_caret);
}
