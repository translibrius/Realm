#include "platform.h"

#ifdef PLATFORM_WINDOWS

#include "util/assert.h"

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct platform_state {
    HINSTANCE handle;
    CONSOLE_SCREEN_BUFFER_INFO std_output_csbi;
    CONSOLE_SCREEN_BUFFER_INFO err_output_csbi;
} platform_state;

static platform_state* state;

u64 platform_system_size() {
    return sizeof(platform_state);
}

b8 platform_system_start(void* memory) {
    state = memory;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &state->std_output_csbi);
    GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &state->err_output_csbi);

    return true;
}

void platform_system_shutdown() {
    state = 0;
}

b8 platform_create_window(const char* title) {
    printf("Creating window for app [%s]", title);
    return true;
}

void platform_console_write(const char* message, LOG_LEVEL level) {
    RL_ASSERT_MSG(state != 0, "Tried using platform_console_write when platform is uninitialized");

    b8 is_error = level == LOG_ERROR || level == LOG_FATAL;
    HANDLE console_handle = GetStdHandle(is_error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi = is_error ? state->err_output_csbi : state->std_output_csbi;

    // Color bit flags:
    // FOREGROUND_* are 1,2,4; FOREGROUND_INTENSITY = bright.
    // BACKGROUND_* are 16,32,64; BACKGROUND_INTENSITY = bright.

    static WORD level_colors[] = {
        /* INFO  */  FOREGROUND_GREEN | FOREGROUND_INTENSITY,
        /* DEBUG */  FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        /* TRACE */  FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        /* WARN  */  FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,   // Yellow
        /* ERROR */  FOREGROUND_RED | FOREGROUND_INTENSITY,
        /* FATAL */  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY) | BACKGROUND_RED
    };

    SetConsoleTextAttribute(console_handle, level_colors[level]);

    OutputDebugStringA(message);
    u64 length = strlen(message);
    LPDWORD number_written = 0;

    WriteConsoleA(console_handle, message, (DWORD)length, number_written, 0);

    SetConsoleTextAttribute(console_handle, csbi.wAttributes);
}

#endif // PLATFORM_WINDOWS