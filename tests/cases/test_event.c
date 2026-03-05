#include "../harness/rl_test.h"

#include "core/event.h"

// --- Shared test helpers ---

static i32 g_callback_counter;
static void *g_last_event_data;
static void *g_last_user_data;

static b8 test_event_callback(void *event_data, void *user_data) {
    g_callback_counter++;
    g_last_event_data = event_data;
    g_last_user_data = user_data;
    return true;
}

// --- Dedicated helpers for isolation ---

static i32 g_wrong_type_counter;

static b8 wrong_type_callback(void *event_data, void *user_data) {
    (void)event_data;
    (void)user_data;
    g_wrong_type_counter++;
    return true;
}

static i32 g_multi_counter;

static b8 multi_callback(void *event_data, void *user_data) {
    (void)event_data;
    (void)user_data;
    g_multi_counter++;
    return false;
}

static i32 g_payload_width;

static b8 payload_callback(void *event_data, void *user_data) {
    (void)user_data;
    e_resize_payload *payload = event_data;
    g_payload_width = payload->width;
    return true;
}

static i32 g_consumption_second_counter;

static b8 consuming_callback(void *event_data, void *user_data) {
    (void)event_data;
    (void)user_data;
    g_callback_counter++;
    return true; // Consume — stop propagation
}

static b8 second_callback(void *event_data, void *user_data) {
    (void)event_data;
    (void)user_data;
    g_consumption_second_counter++;
    return false;
}

// --- Tests ---

RL_TEST(event_register_and_fire_calls_callback) {
    g_callback_counter = 0;

    event_register(EVENT_SPLASH_INCREMENT, test_event_callback, nullptr);
    event_fire(EVENT_SPLASH_INCREMENT, nullptr);

    RL_EXPECT_MSG(g_callback_counter >= 1,
                  "callback should have been called, counter=%d", g_callback_counter);
}

RL_TEST(event_fire_passes_userdata) {
    static i32 user_val = 99;
    g_last_user_data = nullptr;

    event_register(EVENT_WINDOW_FOCUS_GAINED, test_event_callback, &user_val);
    event_fire(EVENT_WINDOW_FOCUS_GAINED, nullptr);

    RL_EXPECT(g_last_user_data == &user_val);
}

RL_TEST(event_fire_wrong_type_does_not_call) {
    g_wrong_type_counter = 0;

    event_register(EVENT_WINDOW_FOCUS_LOST, wrong_type_callback, nullptr);

    // Fire a DIFFERENT event type
    event_fire(EVENT_MOUSE_SCROLL, nullptr);

    RL_EXPECT_EQ_I32(g_wrong_type_counter, 0);
}

RL_TEST(event_multiple_listeners_all_called) {
    g_multi_counter = 0;

    event_register(EVENT_KEY_PRESS, multi_callback, nullptr);
    event_register(EVENT_KEY_PRESS, multi_callback, nullptr);
    event_register(EVENT_KEY_PRESS, multi_callback, nullptr);

    event_fire(EVENT_KEY_PRESS, nullptr);

    RL_EXPECT_MSG(g_multi_counter >= 3,
                  "all 3 listeners should fire, counter=%d", g_multi_counter);
}

RL_TEST(event_fire_passes_payload_data) {
    g_payload_width = 0;

    event_register(EVENT_WINDOW_RESIZE, payload_callback, nullptr);

    e_resize_payload payload = {
        .window_id = 1,
        .x = 0,
        .y = 0,
        .width = 1920,
        .height = 1080,
    };
    event_fire(EVENT_WINDOW_RESIZE, &payload);

    RL_EXPECT_EQ_I32(g_payload_width, 1920);
}

static i32 g_unregister_counter;

static b8 unregister_test_callback(void *event_data, void *user_data) {
    (void)event_data;
    (void)user_data;
    g_unregister_counter++;
    return true;
}

RL_TEST(event_unregister_removes_listener) {
    g_unregister_counter = 0;

    // Use a unique callback+type combo that no other test registers
    event_register(EVENT_WINDOW_FOCUS_LOST, unregister_test_callback, nullptr);
    event_unregister(EVENT_WINDOW_FOCUS_LOST, unregister_test_callback, nullptr);
    event_fire(EVENT_WINDOW_FOCUS_LOST, nullptr);

    RL_EXPECT_EQ_I32(g_unregister_counter, 0);
}

RL_TEST(event_unregister_nonexistent_is_safe) {
    // Unregistering a callback that was never registered should not crash
    event_unregister(EVENT_MOUSE_SCROLL, test_event_callback, nullptr);
    // If we got here, no crash — pass
    RL_EXPECT(true);
}

RL_TEST(event_consumption_stops_propagation) {
    g_callback_counter = 0;
    g_consumption_second_counter = 0;

    // First listener consumes the event (returns true)
    event_register(EVENT_CONFIG_CHANGED, consuming_callback, nullptr);
    // Second listener should never fire
    event_register(EVENT_CONFIG_CHANGED, second_callback, nullptr);

    event_fire(EVENT_CONFIG_CHANGED, nullptr);

    RL_EXPECT_EQ_I32(g_callback_counter, 1);
    RL_EXPECT_MSG(g_consumption_second_counter == 0,
                  "second listener should not fire after consumption, counter=%d",
                  g_consumption_second_counter);
}

void register_event_tests(void) {
    rl_test_begin_group("event");
    RL_REGISTER_TEST(event_register_and_fire_calls_callback);
    RL_REGISTER_TEST(event_fire_passes_userdata);
    RL_REGISTER_TEST(event_fire_wrong_type_does_not_call);
    RL_REGISTER_TEST(event_multiple_listeners_all_called);
    RL_REGISTER_TEST(event_fire_passes_payload_data);
    RL_REGISTER_TEST(event_unregister_removes_listener);
    RL_REGISTER_TEST(event_unregister_nonexistent_is_safe);
    RL_REGISTER_TEST(event_consumption_stops_propagation);
}
