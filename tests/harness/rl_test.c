#include "rl_test.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RL_TEST_MAX_CASES 1024

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

    fprintf(stderr, "    %s:%d: expectation failed: %s", file, line, expression ? expression : "<unknown>");

    if (fmt && fmt[0]) {
        fprintf(stderr, " (");
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, ")");
    }

    fprintf(stderr, "\n");
}

void rl_test_fail_impl(const char *file, i32 line, const char *fmt, ...) {
    g_current_assertions++;
    g_total_assertions++;
    g_current_failures++;
    g_total_failures++;

    fprintf(stderr, "    %s:%d: failure", file, line);
    if (fmt && fmt[0]) {
        fprintf(stderr, " (");
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
        fprintf(stderr, ")");
    }
    fprintf(stderr, "\n");
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
                        "%s=%s %s=%s",
                        actual_expr,
                        actual,
                        expected_expr,
                        expected);
}

i32 rl_test_run_from_args(i32 argc, const char **argv) {
    rl_test_options options = {0};
    if (!rl_test_parse_options(argc, argv, &options)) {
        rl_test_print_usage(argv[0]);
        return 2;
    }

    if (options.list_only) {
        for (u32 i = 0; i < g_case_count; i++) {
            if (rl_test_matches_filter(g_cases[i].name, options.filter)) {
                printf("%s\n", g_cases[i].name);
            }
        }
        return 0;
    }

    u32 passed_count = 0;
    u32 failed_count = 0;
    u32 skipped_count = 0;

    g_total_assertions = 0;
    g_total_failures = 0;

    for (u32 i = 0; i < g_case_count; i++) {
        rl_test_case test_case = g_cases[i];
        if (!rl_test_matches_filter(test_case.name, options.filter)) {
            skipped_count++;
            continue;
        }

        g_current_case = test_case.name;
        g_current_assertions = 0;
        g_current_failures = 0;

        printf("[ RUN  ] %s\n", g_current_case);
        test_case.fn();

        if (g_current_failures == 0) {
            passed_count++;
            printf("[ PASS ] %s (%u assertions)\n", g_current_case, g_current_assertions);
        } else {
            failed_count++;
            printf("[ FAIL ] %s (%u failed of %u assertions)\n",
                   g_current_case,
                   g_current_failures,
                   g_current_assertions);

            if (options.fail_fast) {
                break;
            }
        }
    }

    if ((passed_count + failed_count) == 0) {
        fprintf(stderr, "No tests matched filter\n");
        return 1;
    }

    printf("\n[ DONE ] passed=%u failed=%u skipped=%u assertions=%u\n",
           passed_count,
           failed_count,
           skipped_count,
           g_total_assertions);

    return failed_count > 0 ? 1 : 0;
}
