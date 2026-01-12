#pragma once

#include "core/logger.h"
#include "defines.h"

#include "cglm.h"

#define MAX_WINDOWS 10

enum platform_window_flag {
    WINDOW_FLAG_DEFAULT = 0,
    WINDOW_FLAG_NO_DECORATION = 1 << 0,
    WINDOW_FLAG_NO_INPUT = 1 << 1,
    WINDOW_FLAG_ON_TOP = 1 << 2,
    WINDOW_FLAG_TRANSPARENT = 1 << 3,
};

typedef enum PLATFORM_WINDOW_MODE {
    WINDOW_MODE_WINDOWED,
    WINDOW_MODE_BORDERLESS,
    WINDOW_MODE_FULLSCREEN,
} PLATFORM_WINDOW_MODE;

typedef struct platform_window_settings {
    const char *title;
    i32 x;
    i32 y;
    i32 width;
    i32 height;
    b8 start_center;
    u32 window_flags;
    PLATFORM_WINDOW_MODE window_mode;
} platform_window_settings;

typedef struct platform_window {
    u16 id;
    platform_window_settings settings;
    void *handle;
} platform_window;

struct VK_Context;

typedef enum platform_cursor_mode {
    CURSOR_MODE_NORMAL,
    CURSOR_MODE_HIDDEN,
    CURSOR_MODE_LOCKED
} platform_cursor_mode;

typedef struct platform_info {
    const char *platform_name; // Win32, Linux, MacOS, Android
    u32 build_number;
    u32 version_major;
    u32 version_minor;
    u32 page_size;
    u32 logical_processors;
    u32 alloc_granularity;
    i64 clock_freq;
    const char *arch;
} platform_info;

// System
b8 platform_system_start();
void platform_system_shutdown();

// Memory
void *platform_mem_reserve(u64 size);
b8 platform_mem_commit(void *ptr, u64 size);
b8 platform_mem_decommit(void *ptr, u64 size);
b8 platform_mem_release(void *ptr, u64 size);

// Windowing
b8 platform_pump_messages();
b8 platform_create_window(platform_window *window);
b8 platform_destroy_window(u16 id);
b8 platform_set_window_mode(platform_window *window, PLATFORM_WINDOW_MODE mode);
b8 platform_window_should_close(u16 id);

// Graphics context stuff
b8 platform_create_opengl_context(platform_window *window);
b8 platform_context_make_current(platform_window *window);
b8 platform_swap_buffers(platform_window *window);
u32 platform_get_required_vulkan_extensions(const char ***names_out, b8 enable_validation); // Returns count of extensions
b8 platform_create_vulkan_surface(platform_window *window, struct VK_Context *context);

// Cursor api
void platform_set_cursor_mode(platform_window *window, platform_cursor_mode mode);
b8 platform_set_cursor_position(platform_window *window, vec2 position);
b8 platform_center_cursor(platform_window *window);

// Raw mouse input toggle. Should be only toggled on/off on main window.
b8 platform_set_raw_input(platform_window *window, bool enable);
b8 platform_get_raw_input();

// Misc
i64 platform_get_clock_counter();
void platform_sleep(u32 milliseconds);
u64 platform_get_current_thread_id();
void platform_console_write(const char *message, LOG_LEVEL level);
platform_info *platform_get_info();
void log_system_info();