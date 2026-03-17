#include "rl_test.h"

#include "platform/platform.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
    const char *group;
    rl_test_fn fn;
} rl_test_case;

typedef struct rl_test_options {
    b8 list_only;
    b8 fail_fast;
    b8 quiet;
    const char *filter;
} rl_test_options;

const char *rl_test_tmp_dir(void) {
#ifdef PLATFORM_WINDOWS
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    const char *tmp = getenv("TEMP");
#pragma clang diagnostic pop
    if (tmp && tmp[0]) return tmp;
    return ".";
#else
    return "/tmp";
#endif
}

static rl_test_case g_cases[RL_TEST_MAX_CASES];
static u32 g_case_count;

static const char *g_register_group;

static const char *g_current_case;
static u32 g_current_assertions;
static u32 g_current_failures;
static b8 g_current_skipped;

static u32 g_total_assertions;
static u32 g_total_failures;

static jmp_buf g_test_jmpbuf;

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
    printf("Usage: %s [--list] [--filter <substring>] [--fail-fast] [--quiet]\n", exe_name);
}

static b8 rl_test_parse_options(i32 argc, const char **argv, rl_test_options *out_options) {
    out_options->list_only = false;
    out_options->fail_fast = false;
    out_options->quiet = false;
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
        if (strcmp(arg, "--quiet") == 0) {
            out_options->quiet = true;
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

static const char *rl_test_strip_prefix(const char *name, const char *group) {
    if (!group) {
        return name;
    }
    u32 glen = (u32)strlen(group);
    if (strncmp(name, group, glen) == 0 && name[glen] == '_') {
        return name + glen + 1;
    }
    return name;
}

void rl_test_begin_group(const char *name) {
    g_register_group = name;
}

void rl_test_register(const char *name, rl_test_fn fn) {
    if (g_case_count >= RL_TEST_MAX_CASES) {
        fprintf(stderr, "Too many tests registered (max=%u)\n", RL_TEST_MAX_CASES);
        return;
    }

    g_cases[g_case_count++] = (rl_test_case){
        .name = name,
        .group = g_register_group,
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
            "             " C_RED "x " C_RESET C_OVERLAY0 "%s:%d " C_RESET C_TEXT "%s" C_RESET,
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

    fprintf(stderr, "             " C_RED "x " C_RESET C_OVERLAY0 "%s:%d", file, line);
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

void rl_test_skip_impl(const char *file, i32 line, const char *reason) {
    (void)file;
    (void)line;
    g_current_skipped = true;
    (void)reason;
}

_Noreturn void rl_test_assert_fail_impl(const char *expression,
                                        const char *file,
                                        i32 line,
                                        const char *fmt,
                                        ...) {
    g_current_assertions++;
    g_total_assertions++;
    g_current_failures++;
    g_total_failures++;

    fprintf(stderr,
            "             " C_RED "X " C_RESET C_OVERLAY0 "%s:%d " C_RESET C_TEXT "%s" C_RESET,
            file, line, expression ? expression : "<unknown>");

    if (fmt && fmt[0]) {
        fprintf(stderr, C_OVERLAY0 " -> ");
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, C_RESET);
    }

    fprintf(stderr, C_RED " (fatal)" C_RESET "\n");

    longjmp(g_test_jmpbuf, 1);
}

i32 rl_test_run_from_args(i32 argc, const char **argv) {
    rl_test_enable_ansi();

    rl_test_options options = {0};
    if (!rl_test_parse_options(argc, argv, &options)) {
        rl_test_print_usage(argv[0]);
        return 2;
    }

    if (options.list_only) {
        const char *last_group = nullptr;
        for (u32 i = 0; i < g_case_count; i++) {
            if (!rl_test_matches_filter(g_cases[i].name, options.filter)) {
                continue;
            }
            const char *group = g_cases[i].group;
            if (group && (last_group == nullptr || strcmp(group, last_group) != 0)) {
                if (last_group) {
                    printf("\n");
                }
                printf("  " C_LAVENDER "%s" C_RESET "\n", group);
                last_group = group;
            }
            const char *display = rl_test_strip_prefix(g_cases[i].name, group);
            printf("    %s\n", display);
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
    u32 skipped_count = 0;

    g_total_assertions = 0;
    g_total_failures = 0;

    const char *last_group = nullptr;
    b8 stopped_early = false;

    i64 suite_start = platform_get_clock_counter();

    for (u32 i = 0; i < g_case_count; i++) {
        rl_test_case test_case = g_cases[i];
        if (!rl_test_matches_filter(test_case.name, options.filter)) {
            continue;
        }

        // Group header
        const char *group = test_case.group;
        if (group && (last_group == nullptr || strcmp(group, last_group) != 0)) {
            if (!options.quiet) {
                if (last_group) {
                    printf("\n");
                }
                printf("  " C_LAVENDER "%s" C_RESET "\n", group);
            }
            last_group = group;
        }

        const char *display = rl_test_strip_prefix(test_case.name, group);

        g_current_case = test_case.name;
        g_current_assertions = 0;
        g_current_failures = 0;
        g_current_skipped = false;

        i64 t0 = platform_get_clock_counter();
        if (setjmp(g_test_jmpbuf) == 0) {
            test_case.fn();
        }
        i64 t1 = platform_get_clock_counter();

        double ms = (double)(t1 - t0) / (double)clock_freq * 1000.0;

        if (g_current_skipped) {
            skipped_count++;
            if (!options.quiet) {
                printf("    " C_YELLOW "~ " C_RESET C_TEXT "%s " C_OVERLAY0 "skipped" C_RESET "\n",
                       display);
            }
        } else if (g_current_failures == 0) {
            passed_count++;
            if (!options.quiet) {
                printf("    " C_GREEN "o " C_RESET C_TEXT "%s " C_OVERLAY0 "%u checks",
                       display, g_current_assertions);
                if (ms >= 0.1) {
                    printf("  %.1fms", ms);
                }
                printf(C_RESET "\n");
            }
        } else {
            failed_count++;
            // In quiet mode, print the group header on first failure in that group
            if (options.quiet) {
                // Check if we need to print group header for this failure
                // Walk back to find if any previous test in this group already failed
                b8 group_header_needed = true;
                for (u32 j = 0; j < i; j++) {
                    if (g_cases[j].group && group && strcmp(g_cases[j].group, group) == 0 &&
                        rl_test_matches_filter(g_cases[j].name, options.filter)) {
                        // A previous test existed in this group — was it a failure?
                        // We can't know easily, so just always print the header before first shown test
                        group_header_needed = false;
                        break;
                    }
                }
                if (group_header_needed && group) {
                    printf("  " C_LAVENDER "%s" C_RESET "\n", group);
                }
            }
            printf("    " C_RED "x " C_RESET C_TEXT "%s " C_RED "%u failed" C_OVERLAY0 " / %u checks",
                   display,
                   g_current_failures,
                   g_current_assertions);
            if (ms >= 0.1) {
                printf("  %.1fms", ms);
            }
            printf(C_RESET "\n");

            if (options.fail_fast) {
                printf("\n  " C_YELLOW "stopped early " C_OVERLAY0 "(--fail-fast)" C_RESET "\n");
                stopped_early = true;
                break;
            }
        }
    }

    if ((passed_count + failed_count + skipped_count) == 0) {
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
        printf("  " C_GREEN C_BOLD "all %u tests passed" C_RESET, passed_count);
    } else {
        printf("  " C_RED C_BOLD "%u of %u tests failed" C_RESET, failed_count, passed_count + failed_count);
    }
    if (skipped_count > 0) {
        printf(C_YELLOW "  %u skipped" C_RESET, skipped_count);
    }
    printf("\n");

    printf("  " C_OVERLAY0 "%u assertions  %.0fms" C_RESET "\n\n", g_total_assertions, suite_ms);

    return failed_count > 0 ? 1 : 0;
}
