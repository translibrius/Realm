#pragma once

#ifdef __cplusplus
extern "C" {



#endif

// Unsigned int types.
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

// Signed int types.
typedef signed char i8;
typedef signed short i16;
typedef signed int i32;
typedef signed long long i64;

// Floating point types
typedef float f32;
typedef double f64;

// Boolean type
#ifdef __cplusplus
typedef bool b8;
#else
typedef _Bool b8;
#endif

typedef int b32;

#ifdef __cplusplus
}
#endif

// Properly define static assertions.
#if defined(__clang__) || defined(__gcc__)
#define STATIC_ASSERT _Static_assert
#else
#define STATIC_ASSERT static_assert
#endif

// Ensure all types are of the correct size.
STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 byte.");
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

#if defined(_DEBUG)
#define RL_BUILD_DEBUG 1
#else
#define RL_BUILD_DEBUG 0
#endif

#if defined(_WIN32)

#if defined(ENGINE_SHARED)
#if defined(ENGINE_BUILD)
#define REALM_API __declspec(dllexport)
#else
#define REALM_API __declspec(dllimport)
#endif
#else
// Static build
#define REALM_API
#endif

#else
#define REALM_API
#endif


// Inlining
#ifdef _MSC_VER
#define RL_INLINE __forceinline
#define RL_NOINLINE __declspec(noinline)
#else
#define RL_INLINE static inline
#define RL_NOINLINE
#endif

// Platform detection
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#define PLATFORM_WINDOWS 1
#define VK_USE_PLATFORM_WIN32_KHR
#ifndef _WIN64
#error "64-bit is required on Windows!"
#endif
#elif defined(__linux__) || defined(__gnu_linux__)
// Linux OS
#define PLATFORM_LINUX 1
#  if defined(__ANDROID__)
#       define PLATFORM_ANDROID 1
#  endif
#else
#error "Only windows is supported for now"
#endif

// -- Memory helpers
#define GiB(bytes) (u64) (bytes * 1024 * 1024 * 1024)
#define MiB(bytes) (u64) (bytes * 1024 * 1024)
#define KiB(bytes) (u64) (bytes * 1024)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))