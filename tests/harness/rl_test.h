#pragma once

#include "defines.h"

#include <math.h>

typedef void (*rl_test_fn)(void);

void rl_test_begin_group(const char *name);
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
void rl_test_skip_impl(const char *file, i32 line, const char *reason);
_Noreturn void rl_test_assert_fail_impl(const char *expression,
                                        const char *file,
                                        i32 line,
                                        const char *fmt,
                                        ...);

#define RL_TEST(name) static void name(void)
#define RL_REGISTER_TEST(name) rl_test_register(#name, name)

// --- non-fatal assertions (test continues) ---

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

#define RL_EXPECT_EQ_U64(actual, expected) \
    do { \
        u64 _actual_value = (u64)(actual); \
        u64 _expected_value = (u64)(expected); \
        rl_test_expect_impl(_actual_value == _expected_value, \
                            #actual " == " #expected, \
                            __FILE__, \
                            __LINE__, \
                            "actual=%llu expected=%llu", \
                            _actual_value, \
                            _expected_value); \
    } while (0)

#define RL_EXPECT_STR_EQ(actual, expected) \
    rl_test_expect_str_eq_impl((actual), (expected), #actual, #expected, __FILE__, __LINE__)

#define RL_EXPECT_NEAR_F32(actual, expected, epsilon) \
    do { \
        f32 _a = (f32)(actual); \
        f32 _e = (f32)(expected); \
        f32 _eps = (f32)(epsilon); \
        rl_test_expect_impl(fabsf(_a - _e) <= _eps, \
                            #actual " ~= " #expected, \
                            __FILE__, \
                            __LINE__, \
                            "actual=%f expected=%f epsilon=%f", \
                            (double)_a, (double)_e, (double)_eps); \
    } while (0)

#define RL_EXPECT_NEAR_F64(actual, expected, epsilon) \
    do { \
        f64 _a = (f64)(actual); \
        f64 _e = (f64)(expected); \
        f64 _eps = (f64)(epsilon); \
        rl_test_expect_impl(fabs(_a - _e) <= _eps, \
                            #actual " ~= " #expected, \
                            __FILE__, \
                            __LINE__, \
                            "actual=%f expected=%f epsilon=%f", \
                            _a, _e, _eps); \
    } while (0)

#define RL_EXPECT_NOT_NULL(ptr) \
    rl_test_expect_impl((ptr) != nullptr, #ptr " != NULL", __FILE__, __LINE__, nullptr)

#define RL_EXPECT_NULL(ptr) \
    rl_test_expect_impl((ptr) == nullptr, #ptr " == NULL", __FILE__, __LINE__, nullptr)

// --- skip (stops current test, not a failure) ---

#define RL_SKIP(reason) \
    do { rl_test_skip_impl(__FILE__, __LINE__, reason); return; } while (0)

// --- fatal assertions (stops current test on failure) ---
// Named RL_TEST_ASSERT to avoid collision with engine's RL_ASSERT (debugBreak).

#define RL_TEST_ASSERT(expr) \
    do { \
        if (!(expr)) { \
            rl_test_assert_fail_impl(#expr, __FILE__, __LINE__, nullptr); \
        } \
    } while (0)

#define RL_TEST_ASSERT_MSG(expr, fmt, ...) \
    do { \
        if (!(expr)) { \
            rl_test_assert_fail_impl(#expr, __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while (0)
