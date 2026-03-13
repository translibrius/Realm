#include "../harness/rl_test.h"
#include "../support/test_runtime.h"

#include "core/logger.h"
#include "util/str.h"

#include <string.h>

// --- Callback helpers ---

static LOG_LEVEL g_last_cb_level;
static char g_last_cb_text[256];
static i32 g_cb_count;

static void test_logger_callback(LOG_LEVEL level, const char *text, u16 len, void *userdata) {
    (void)userdata;
    (void)len;
    g_last_cb_level = level;
    cstr_copy(g_last_cb_text, sizeof(g_last_cb_text), text);
    g_cb_count++;
}

// --- Tests ---

RL_TEST(logger_level_get_set) {
    logger_set_level(LOG_WARN);
    RL_EXPECT_EQ_I32((i32)logger_get_level(), (i32)LOG_WARN);

    logger_set_level(LOG_ERROR);
    RL_EXPECT_EQ_I32((i32)logger_get_level(), (i32)LOG_ERROR);

    // Restore to suppress noise
    logger_set_level(LOG_FATAL);
}

RL_TEST(logger_callback_receives_level_and_message) {
    g_cb_count = 0;
    g_last_cb_text[0] = '\0';
    logger_set_level(LOG_INFO);
    logger_set_callback(test_logger_callback, nullptr);

    // Suppress console so async writer doesn't pollute harness output.
    rl_test_suppress_console();
    log_output("hello from test", LOG_INFO, __func__);
    rl_test_restore_console();

    // The callback fires synchronously before the async writer — no sleep needed.
    RL_EXPECT_MSG(g_cb_count >= 1, "callback should have fired, count=%d", g_cb_count);
    RL_EXPECT_EQ_I32((i32)g_last_cb_level, (i32)LOG_INFO);
    RL_EXPECT_MSG(strstr(g_last_cb_text, "hello from test") != nullptr,
                  "callback text should contain message, got: '%s'", g_last_cb_text);

    logger_set_level(LOG_FATAL);
    logger_set_callback(nullptr, nullptr);
}

RL_TEST(logger_level_filters_below_threshold) {
    g_cb_count = 0;
    logger_set_callback(test_logger_callback, nullptr);
    logger_set_level(LOG_WARN);

    // INFO is below WARN threshold — callback should not fire.
    rl_test_suppress_console();
    log_output("should not appear", LOG_INFO, __func__);
    rl_test_restore_console();
    RL_EXPECT_EQ_I32(g_cb_count, 0);

    // WARN meets threshold — callback should fire.
    rl_test_suppress_console();
    log_output("warning message", LOG_WARN, __func__);
    rl_test_restore_console();
    RL_EXPECT_MSG(g_cb_count >= 1, "WARN should pass filter, count=%d", g_cb_count);
    RL_EXPECT_EQ_I32((i32)g_last_cb_level, (i32)LOG_WARN);

    logger_set_level(LOG_FATAL);
    logger_set_callback(nullptr, nullptr);
}

void register_logger_tests(void) {
    rl_test_begin_group("logger");
    RL_REGISTER_TEST(logger_level_get_set);
    RL_REGISTER_TEST(logger_callback_receives_level_and_message);
    RL_REGISTER_TEST(logger_level_filters_below_threshold);
}
