#include "../harness/rl_test.h"

#include "core/config.h"
#include "memory/memory.h"
#include "platform/io/file_io.h"

#include <stdlib.h>
#include <string.h>

static void *g_config_mem;

static void config_test_setup(void) {
    g_config_mem = mem_alloc(config_system_size(), MEM_SUBSYSTEM_CONFIG);
    // Remove any leftover config file
    platform_file_delete(RL_CONFIG_FILENAME);
}

static void config_test_teardown(void) {
    config_system_shutdown();
    mem_free(g_config_mem, config_system_size(), MEM_SUBSYSTEM_CONFIG);
    g_config_mem = nullptr;
    platform_file_delete(RL_CONFIG_FILENAME);
}

RL_TEST(config_defaults_are_sane) {
    rl_config defaults = config_defaults();

    RL_EXPECT_EQ_I32(defaults.window_width, 500);
    RL_EXPECT_EQ_I32(defaults.window_height, 500);
    RL_EXPECT_EQ_I32(defaults.window_x, 0);
    RL_EXPECT_EQ_I32(defaults.window_y, 0);
    RL_EXPECT_EQ_I32(defaults.window_mode, WINDOW_MODE_WINDOWED);
    RL_EXPECT_EQ_I32(defaults.renderer_backend, BACKEND_OPENGL);
    RL_EXPECT(!defaults.vsync);
    RL_EXPECT_EQ_I32(defaults.log_level, LOG_INFO);
}

RL_TEST(config_load_missing_file_uses_defaults) {
    config_test_setup();

    RL_EXPECT(config_system_start(g_config_mem));

    rl_config *cfg = config_get();
    RL_EXPECT(cfg != nullptr);
    RL_EXPECT_EQ_I32(cfg->window_width, 500);
    RL_EXPECT_EQ_I32(cfg->window_height, 500);
    RL_EXPECT_EQ_I32(cfg->renderer_backend, BACKEND_OPENGL);

    config_test_teardown();
}

RL_TEST(config_save_and_roundtrip) {
    config_test_setup();

    RL_EXPECT(config_system_start(g_config_mem));

    rl_config *cfg = config_get();
    cfg->window_width = 1280;
    cfg->window_height = 720;
    cfg->window_x = 42;
    cfg->window_y = 84;
    cfg->window_mode = WINDOW_MODE_BORDERLESS;
    cfg->renderer_backend = BACKEND_VULKAN;
    cfg->vsync = true;
    cfg->log_level = LOG_DEBUG;

    RL_EXPECT(config_save());

    // Shutdown and restart to verify round-trip
    config_system_shutdown();
    mem_zero(g_config_mem, config_system_size());

    RL_EXPECT(config_system_start(g_config_mem));

    cfg = config_get();
    RL_EXPECT_EQ_I32(cfg->window_width, 1280);
    RL_EXPECT_EQ_I32(cfg->window_height, 720);
    RL_EXPECT_EQ_I32(cfg->window_x, 42);
    RL_EXPECT_EQ_I32(cfg->window_y, 84);
    RL_EXPECT_EQ_I32(cfg->window_mode, WINDOW_MODE_BORDERLESS);
    RL_EXPECT_EQ_I32(cfg->renderer_backend, BACKEND_VULKAN);
    RL_EXPECT(cfg->vsync);
    RL_EXPECT_EQ_I32(cfg->log_level, LOG_DEBUG);

    config_test_teardown();
}

RL_TEST(config_load_partial_keys_uses_defaults_for_missing) {
    config_test_setup();

    // Write a config with only some keys
    const char *partial_toml =
        "[window]\n"
        "width = 800\n"
        "\n"
        "[renderer]\n"
        "vsync = true\n";

    RL_EXPECT(platform_file_write_all(RL_CONFIG_FILENAME, partial_toml, strlen(partial_toml)));

    RL_EXPECT(config_system_start(g_config_mem));

    rl_config *cfg = config_get();
    RL_EXPECT_EQ_I32(cfg->window_width, 800);
    RL_EXPECT(cfg->vsync);
    // Missing keys should have defaults
    RL_EXPECT_EQ_I32(cfg->window_height, 500);
    RL_EXPECT_EQ_I32(cfg->renderer_backend, BACKEND_OPENGL);
    RL_EXPECT_EQ_I32(cfg->log_level, LOG_INFO);

    config_test_teardown();
}

RL_TEST(config_load_corrupt_file_uses_defaults) {
    config_test_setup();

    const char *garbage = "{{{{ not valid toml !@#$%\n\xff\xfe";

    RL_EXPECT(platform_file_write_all(RL_CONFIG_FILENAME, garbage, strlen(garbage)));

    RL_EXPECT(config_system_start(g_config_mem));

    rl_config *cfg = config_get();
    RL_EXPECT(cfg != nullptr);
    // Should still have defaults
    RL_EXPECT_EQ_I32(cfg->window_width, 500);
    RL_EXPECT_EQ_I32(cfg->window_height, 500);

    config_test_teardown();
}

RL_TEST(config_load_unknown_enum_uses_default) {
    config_test_setup();

    const char *bad_enum =
        "[window]\n"
        "width = 800\n"
        "\n"
        "[renderer]\n"
        "backend = \"directx12\"\n"
        "\n"
        "[engine]\n"
        "log_level = \"verbose\"\n";

    RL_EXPECT(platform_file_write_all(RL_CONFIG_FILENAME, bad_enum, strlen(bad_enum)));

    RL_EXPECT(config_system_start(g_config_mem));

    rl_config *cfg = config_get();
    RL_EXPECT_EQ_I32(cfg->window_width, 800);
    // Unknown enum strings should fall back to defaults
    RL_EXPECT_EQ_I32(cfg->renderer_backend, BACKEND_OPENGL);
    RL_EXPECT_EQ_I32(cfg->log_level, LOG_INFO);

    config_test_teardown();
}

void register_config_tests(void) {
    RL_REGISTER_TEST(config_defaults_are_sane);
    RL_REGISTER_TEST(config_load_missing_file_uses_defaults);
    RL_REGISTER_TEST(config_save_and_roundtrip);
    RL_REGISTER_TEST(config_load_partial_keys_uses_defaults_for_missing);
    RL_REGISTER_TEST(config_load_corrupt_file_uses_defaults);
    RL_REGISTER_TEST(config_load_unknown_enum_uses_default);
}
