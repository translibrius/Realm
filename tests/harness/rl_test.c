#include "rl_test.h"

#include "platform/platform.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define RL_TEST_MAX_CASES 1024

// Catppuccin Mocha palette (true-color ANSI)
#define C_RESET     "\x1b[0m"
#define C_BOLD      "\x1b[1m"
#define C_DIM       "\x1b[2m"

#define C_TEXT      "\x1b[38;2;205;214;244m" // #cdd6f4  text
#define C_SUBTEXT1  "\x1b[38;2;186;194;222m" // #bac2de  subtext1
#define C_OVERLAY0  "\x1b[38;2;108;112;134m" // #6c7086  overlay0
#define C_GREEN     "\x1b[38;2;166;227;161m" // #a6e3a1  green
#define C_RED       "\x1b[38;2;243;139;168m" // #f38ba8  red
#define C_YELLOW    "\x1b[38;2;249;226;175m" // #f9e2af  yellow
#define C_LAVENDER  "\x1b[38;2;180;190;254m" // #b4befe  lavender

typedef struct rl_test_case {
    const char *name;
    rl_test_fn fn;
} rl_test_case;

typedef struct rl_test_options {
    b8 list_only;
    b8 fail_fast;
    const char *filter;
} rl_test_options;

static rl_test_case g_cases[RL_TEST_MAX_CASES];
static u32 g_case_count;

static const char *g_current_case;
static u32 g_current_assertions;
static u32 g_current_failures;

static u32 g_total_assertions;
static u32 g_total_failures;

static void rl_test_enable_ansi(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);

    HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
    DWORD mode = 0;
    if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode)) {
        SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
    if (err != INVALID_HANDLE_VALUE && GetConsoleMode(err, &mode)) {
        SetConsoleMode(err, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

static void rl_test_print_usage(const char *exe_name) {
    printf("Usage: %s [--list] [--filter <substring>] [--fail-fast]\n", exe_name);
}

static b8 rl_test_parse_options(i32 argc, const char **argv, rl_test_options *out_options) {
    out_options->list_only = false;
    out_options->fail_fast = false;
    out_options->filter = nullptr;

    for (i32 i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--list") == 0) {
            out_options->list_only = true;
            continue;
        }
        if (strcmp(arg, "--fail-fast") == 0) {
            out_options->fail_fast = true;
            continue;
        }
        if (strcmp(arg, "--filter") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing value for --filter\n");
                return false;
            }
            out_options->filter = argv[++i];
            continue;
        }

        fprintf(stderr, "Unknown argument: %s\n", arg);
        return false;
    }

    return true;
}

static b8 rl_test_matches_filter(const char *name, const char *filter) {
    if (!filter || !filter[0]) {
        return true;
    }
    return strstr(name, filter) != nullptr;
}

void rl_test_register(const char *name, rl_test_fn fn) {
    if (g_case_count >= RL_TEST_MAX_CASES) {
        fprintf(stderr, "Too many tests registered (max=%u)\n", RL_TEST_MAX_CASES);
        return;
    }

    g_cases[g_case_count++] = (rl_test_case){
        .name = name,
        .fn = fn,
    };
}

void rl_test_expect_impl(b8 condition,
                         const char *expression,
                         const char *file,
                         i32 line,
                         const char *fmt,
                         ...) {
    g_current_assertions++;
    g_total_assertions++;

    if (condition) {
        return;
    }

    g_current_failures++;
    g_total_failures++;

    fprintf(stderr,
            "         " C_RED "x " C_RESET C_OVERLAY0 "%s:%d " C_RESET C_TEXT "%s" C_RESET,
            file, line, expression ? expression : "<unknown>");

    if (fmt && fmt[0]) {
        fprintf(stderr, C_OVERLAY0 " -> ");
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, C_RESET);
    }

    fprintf(stderr, "\n");
}

void rl_test_fail_impl(const char *file, i32 line, const char *fmt, ...) {
    g_current_assertions++;
    g_total_assertions++;
    g_current_failures++;
    g_total_failures++;

    fprintf(stderr, "         " C_RED "x " C_RESET C_OVERLAY0 "%s:%d", file, line);
    if (fmt && fmt[0]) {
        fprintf(stderr, " -> ");
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
    fprintf(stderr, C_RESET "\n");
}

void rl_test_expect_str_eq_impl(const char *actual,
                                const char *expected,
                                const char *actual_expr,
                                const char *expected_expr,
                                const char *file,
                                i32 line) {
    if (actual == expected) {
        rl_test_expect_impl(true, "string equality", file, line, NULL);
        return;
    }

    if (!actual || !expected) {
        rl_test_expect_impl(false,
                            "string equality",
                            file,
                            line,
                            "%s=%s %s=%s",
                            actual_expr,
                            actual ? actual : "<null>",
                            expected_expr,
                            expected ? expected : "<null>");
        return;
    }

    rl_test_expect_impl(strcmp(actual, expected) == 0,
                        "string equality",
                        file,
                        line,
                        "%s=\"%s\" %s=\"%s\"",
                        actual_expr,
                        actual,
                        expected_expr,
                        expected);
}

i32 rl_test_run_from_args(i32 argc, const char **argv) {
    rl_test_enable_ansi();

    rl_test_options options = {0};
    if (!rl_test_parse_options(argc, argv, &options)) {
        rl_test_print_usage(argv[0]);
        return 2;
    }

    if (options.list_only) {
        for (u32 i = 0; i < g_case_count; i++) {
            if (rl_test_matches_filter(g_cases[i].name, options.filter)) {
                printf("  %s\n", g_cases[i].name);
            }
        }
        return 0;
    }

    i64 clock_freq = platform_get_info()->clock_freq;

    // Count matched tests for header
    u32 matched_count = 0;
    for (u32 i = 0; i < g_case_count; i++) {
        if (rl_test_matches_filter(g_cases[i].name, options.filter)) {
            matched_count++;
        }
    }

    // Header
    printf("\n");
    printf("  " C_LAVENDER C_BOLD "Realm" C_RESET C_SUBTEXT1 " test suite" C_RESET "\n");
    if (options.filter) {
        printf("  " C_OVERLAY0 "%u of %u test(s) matched '%s'" C_RESET "\n",
               matched_count, g_case_count, options.filter);
    } else {
        printf("  " C_OVERLAY0 "%u test(s) registered" C_RESET "\n", g_case_count);
    }
    printf("\n");

    u32 passed_count = 0;
    u32 failed_count = 0;

    g_total_assertions = 0;
    g_total_failures = 0;

    i64 suite_start = platform_get_clock_counter();

    for (u32 i = 0; i < g_case_count; i++) {
        rl_test_case test_case = g_cases[i];
        if (!rl_test_matches_filter(test_case.name, options.filter)) {
            continue;
        }

        g_current_case = test_case.name;
        g_current_assertions = 0;
        g_current_failures = 0;

        i64 t0 = platform_get_clock_counter();
        test_case.fn();
        i64 t1 = platform_get_clock_counter();

        double ms = (double)(t1 - t0) / (double)clock_freq * 1000.0;

        if (g_current_failures == 0) {
            passed_count++;
            printf("  " C_GREEN "o " C_RESET C_TEXT "%s " C_OVERLAY0 "%u checks",
                   g_current_case, g_current_assertions);
            if (ms >= 0.1) {
                printf("  %.1fms", ms);
            }
            printf(C_RESET "\n");
        } else {
            failed_count++;
            printf("  " C_RED "x " C_RESET C_TEXT "%s " C_RED "%u failed" C_OVERLAY0 " / %u checks",
                   g_current_case,
                   g_current_failures,
                   g_current_assertions);
            if (ms >= 0.1) {
                printf("  %.1fms", ms);
            }
            printf(C_RESET "\n");

            if (options.fail_fast) {
                printf("\n  " C_YELLOW "stopped early " C_OVERLAY0 "(--fail-fast)" C_RESET "\n");
                break;
            }
        }
    }

    if ((passed_count + failed_count) == 0) {
        printf("\n  " C_YELLOW "no tests matched filter" C_RESET "\n\n");
        return 1;
    }

    i64 suite_end = platform_get_clock_counter();
    double suite_ms = (double)(suite_end - suite_start) / (double)clock_freq * 1000.0;

    // Summary
    printf("\n");
    printf("  " C_OVERLAY0 "---" C_RESET "\n");
    printf("\n");

    if (failed_count == 0) {
        printf("  " C_GREEN C_BOLD "all %u tests passed" C_RESET "\n", passed_count);
    } else {
        printf("  " C_RED C_BOLD "%u of %u tests failed" C_RESET "\n", failed_count, passed_count + failed_count);
    }

    printf("  " C_OVERLAY0 "%u assertions  %.0fms" C_RESET "\n\n", g_total_assertions, suite_ms);

    return failed_count > 0 ? 1 : 0;
}
