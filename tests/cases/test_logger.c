#include "../harness/rl_test.h"

#include "core/logger.h"
#include "memory/memory.h"
#include "util/str.h"

// --- Per-test logger lifecycle ---

static void *g_logger_mem;

static void logger_test_setup(void) {
    g_logger_mem = mem_alloc(logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    logger_system_start(g_logger_mem);
}

static void logger_test_teardown(void) {
    logger_system_shutdown();
    mem_free(g_logger_mem, logger_system_size(), MEM_SUBSYSTEM_LOGGER);
    g_logger_mem = nullptr;
}

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
    logger_test_setup();

    logger_set_level(LOG_WARN);
    RL_EXPECT_EQ_I32((i32)logger_get_level(), (i32)LOG_WARN);

    logger_set_level(LOG_ERROR);
    RL_EXPECT_EQ_I32((i32)logger_get_level(), (i32)LOG_ERROR);

    logger_test_teardown();
}

RL_TEST(logger_callback_receives_messages) {
    logger_test_setup();

    g_cb_count = 0;
    g_last_cb_text[0] = '\0';
    logger_set_callback(test_logger_callback, nullptr);

    RL_INFO("test message %d", 42);

    RL_EXPECT_MSG(g_cb_count >= 1, "callback should have fired, count=%d", g_cb_count);
    RL_EXPECT_EQ_I32((i32)g_last_cb_level, (i32)LOG_INFO);

    logger_test_teardown();
}

RL_TEST(logger_level_filters_below_threshold) {
    logger_test_setup();

    g_cb_count = 0;
    logger_set_callback(test_logger_callback, nullptr);
    logger_set_level(LOG_WARN);

    // INFO is below WARN threshold — should be filtered
    RL_INFO("should not appear");

    RL_EXPECT_EQ_I32(g_cb_count, 0);

    // WARN should still go through
    RL_WARN("should appear");
    RL_EXPECT_MSG(g_cb_count >= 1, "WARN should pass filter, count=%d", g_cb_count);

    logger_test_teardown();
}

void register_logger_tests(void) {
    RL_REGISTER_TEST(logger_level_get_set);
    RL_REGISTER_TEST(logger_callback_receives_messages);
    RL_REGISTER_TEST(logger_level_filters_below_threshold);
}
