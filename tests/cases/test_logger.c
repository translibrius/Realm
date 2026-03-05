#include "../harness/rl_test.h"
#include "../support/test_runtime.h"

#include "core/logger.h"
#include "platform/platform.h"
#include "util/str.h"

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

// Emit log messages with console suppressed so the async writer thread
// doesn't interleave with harness output, then drain and restore.
static void emit_and_drain(LOG_LEVEL level, const char *msg) {
    rl_test_suppress_console();
    log_output(msg, level, __func__);
    // Let the async writer drain into /dev/null
    platform_sleep(1);
    rl_test_restore_console();
}

// --- Tests ---

RL_TEST(logger_level_get_set) {
    logger_set_level(LOG_WARN);
    RL_EXPECT_EQ_I32((i32)logger_get_level(), (i32)LOG_WARN);

    logger_set_level(LOG_ERROR);
    RL_EXPECT_EQ_I32((i32)logger_get_level(), (i32)LOG_ERROR);

    logger_set_level(LOG_FATAL);
}

RL_TEST(logger_callback_receives_messages) {
    g_cb_count = 0;
    g_last_cb_text[0] = '\0';
    logger_set_level(LOG_INFO);
    logger_set_callback(test_logger_callback, nullptr);

    emit_and_drain(LOG_INFO, "test message %d");

    RL_EXPECT_MSG(g_cb_count >= 1, "callback should have fired, count=%d", g_cb_count);
    RL_EXPECT_EQ_I32((i32)g_last_cb_level, (i32)LOG_INFO);

    logger_set_level(LOG_FATAL);
    logger_set_callback(nullptr, nullptr);
}

RL_TEST(logger_level_filters_below_threshold) {
    g_cb_count = 0;
    logger_set_callback(test_logger_callback, nullptr);
    logger_set_level(LOG_WARN);

    // INFO is below WARN threshold — should be filtered
    emit_and_drain(LOG_INFO, "should not appear");
    RL_EXPECT_EQ_I32(g_cb_count, 0);

    // WARN should still go through
    emit_and_drain(LOG_WARN, "should appear");
    RL_EXPECT_MSG(g_cb_count >= 1, "WARN should pass filter, count=%d", g_cb_count);

    logger_set_level(LOG_FATAL);
    logger_set_callback(nullptr, nullptr);
}

void register_logger_tests(void) {
    RL_REGISTER_TEST(logger_level_get_set);
    RL_REGISTER_TEST(logger_callback_receives_messages);
    RL_REGISTER_TEST(logger_level_filters_below_threshold);
}
