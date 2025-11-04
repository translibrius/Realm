#pragma once

#include "defines.h"

// Disable assertions by commenting out the below line.
#define REALM_ASSERTIONS_ENABLED

#ifdef REALM_ASSERTIONS_ENABLED
#if _MSC_VER
#include <intrin.h>
#define debugBreak() __debugbreak()
#else
#define debugBreak() __builtin_trap()
#endif

REALM_API void report_assertion_failure(const char* expression, const char* message, const char* file, i32 line);

#define RL_ASSERT(expr)                                                \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            debugBreak();                                            \
        }                                                            \
    }

#define RL_ASSERT_MSG(expr, message)                                        \
    {                                                                     \
        if (expr) {                                                       \
        } else {                                                          \
            report_assertion_failure(#expr, message, __FILE__, __LINE__); \
            debugBreak();                                                 \
        }                                                                 \
    }

#ifdef _DEBUG
#define RL_ASSERT_DEBUG(expr)                                          \
    {                                                                \
        if (expr) {                                                  \
        } else {                                                     \
            report_assertion_failure(#expr, "", __FILE__, __LINE__); \
            debugBreak();                                            \
        }                                                            \
    }
#else
#define RL_ASSERT_DEBUG(expr)  // Does nothing at all
#endif

#else
#define RL_ASSERT(expr)               // Does nothing at all
#define RL_ASSERT_MSG(expr, message)  // Does nothing at all
#define RL_ASSERT_DEBUG(expr)         // Does nothing at all
#endif