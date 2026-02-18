#pragma once

#include "defines.h"

typedef void (*rl_test_fn)(void);

void rl_test_register(const char *name, rl_test_fn fn);
i32 rl_test_run_from_args(i32 argc, const char **argv);

void rl_test_expect_impl(b8 condition,
                         const char *expression,
                         const char *file,
                         i32 line,
                         const char *fmt,
                         ...);
void rl_test_fail_impl(const char *file, i32 line, const char *fmt, ...);
void rl_test_expect_str_eq_impl(const char *actual,
                                const char *expected,
                                const char *actual_expr,
                                const char *expected_expr,
                                const char *file,
                                i32 line);

#define RL_TEST(name) static void name(void)
#define RL_REGISTER_TEST(name) rl_test_register(#name, name)

#define RL_EXPECT(expr) \
    rl_test_expect_impl((expr), #expr, __FILE__, __LINE__, nullptr)

#define RL_EXPECT_MSG(expr, fmt, ...) \
    rl_test_expect_impl((expr), #expr, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define RL_FAIL(fmt, ...) \
    rl_test_fail_impl(__FILE__, __LINE__, fmt, ##__VA_ARGS__)

#define RL_EXPECT_EQ_I32(actual, expected) \
    do { \
        i32 _actual_value = (i32)(actual); \
        i32 _expected_value = (i32)(expected); \
        rl_test_expect_impl(_actual_value == _expected_value, \
                            #actual " == " #expected, \
                            __FILE__, \
                            __LINE__, \
                            "actual=%d expected=%d", \
                            _actual_value, \
                            _expected_value); \
    } while (0)

#define RL_EXPECT_EQ_U32(actual, expected) \
    do { \
        u32 _actual_value = (u32)(actual); \
        u32 _expected_value = (u32)(expected); \
        rl_test_expect_impl(_actual_value == _expected_value, \
                            #actual " == " #expected, \
                            __FILE__, \
                            __LINE__, \
                            "actual=%u expected=%u", \
                            _actual_value, \
                            _expected_value); \
    } while (0)

#define RL_EXPECT_STR_EQ(actual, expected) \
    rl_test_expect_str_eq_impl((actual), (expected), #actual, #expected, __FILE__, __LINE__)
