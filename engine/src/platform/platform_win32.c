#include "platform.h"

#ifdef PLATFORM_WINDOWS

#include "util/assert.h"
#include "memory/memory.h"

#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct platform_state {
    HINSTANCE handle;
    CONSOLE_SCREEN_BUFFER_INFO std_output_csbi;
    CONSOLE_SCREEN_BUFFER_INFO err_output_csbi;
} platform_state;

static platform_state state;

b8 platform_system_start() {
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &state.std_output_csbi);
    GetConsoleScreenBufferInfo(GetStdHandle(STD_ERROR_HANDLE), &state.err_output_csbi);

    return true;
}

void platform_system_shutdown() {
    
}

b8 platform_create_window(const char* title) {
    printf("Creating window for app [%s]", title);
    return true;
}

void platform_console_write(const char* message, LOG_LEVEL level) {
    b8 is_error = level == LOG_ERROR || level == LOG_FATAL;
    HANDLE console_handle = GetStdHandle(is_error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi = is_error ? state.err_output_csbi : state.std_output_csbi;

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

/*
static LPCWSTR cstr_to_wcstr(const char* str) {
    if (!str) {
        return 0;
    }

    i32 len = MultiByteToWideChar(CP_UTF8, 0, str, -1, 0, 0);
    if (len == 0) {
        return 0;
    }
    LPWSTR wstr = rl_alloc(sizeof(WCHAR) * len, MEM_STRING);
    if (!wstr) {
        return 0;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr, len) == 0) {
        rl_free(wstr, sizeof(WCHAR) * len, MEM_STRING);
        return 0;
    }
    return wstr;
}

static void wcstr_free(LPCWSTR wstr) {
    if (wstr) {
        u32 len = lstrlenW(wstr); // Note that lstrlen doesn't account for the null terminator.
        rl_free((WCHAR*)wstr, sizeof(WCHAR) * (len + 1), MEM_STRING);
    }
}

static const char* wcstr_to_cstr(LPCWSTR wstr) {
    if (!wstr) {
        return 0;
    }

    i32 length = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (length == 0) {
        return 0;
    }
    char* str = rl_alloc(sizeof(char) * length, MEM_STRING);
    if (!str) {
        return 0;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, length, NULL, NULL) == 0) {
        rl_free((char*)str, sizeof(char) * length, MEM_STRING);
        return 0;
    }

    return str;
}
*/

#endif // PLATFORM_WINDOWS