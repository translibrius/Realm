#pragma once

#include "util/assert.h"
#include "memory/memory.h"

// Define the structure
#define DA_DEFINE(name, type)                   \
    typedef struct {                            \
        type *items;                            \
        u64 count;                              \
        u64 capacity;                           \
    } name

#define RESIZE_FACTOR 2
#define DEFAULT_CAPACITY 12

#define da_init_with_cap(xp, cap) do {                          \
    u64 _cap = (cap);                                           \
    (xp)->capacity = _cap;                                      \
    (xp)->count = 0;                                            \
    (xp)->items = _cap                                          \
    ? rl_alloc(_cap * sizeof(*(xp)->items), MEM_DYNAMIC_ARRAY)  \
    : nullptr;                                                  \
    RL_ASSERT((xp)->items || !_cap);                            \
} while (0)

#define da_init(xp) da_init_with_cap(xp, 0)

// Next capacity: 1.5× for small sizes, 2× after 1024
#define next_capacity(curr) \
((curr) ? (((curr) < 1024) ? ((curr) * 3) / 2 : ((curr) << 1)) : DEFAULT_CAPACITY)


#define da_append(xp, x)                                                        \
    do {                                                                        \
        if ((xp)->count >= (xp)->capacity) {                                    \
            u64 old_cap = (xp)->capacity;                                       \
            u64 new_cap = next_capacity(old_cap);                               \
                                                                                \
            void *ptr = rl_realloc(                                             \
                (xp)->items,                                                    \
                old_cap * sizeof(*(xp)->items),                                 \
                new_cap * sizeof(*(xp)->items),                                 \
                MEM_DYNAMIC_ARRAY                                               \
            );                                                                  \
                                                                                \
            RL_ASSERT(ptr);                                                     \
                                                                                \
            (xp)->items = ptr;                                                  \
            (xp)->capacity = new_cap;                                           \
        }                                                                       \
                                                                                \
        (xp)->items[(xp)->count++] = (x);                                       \
    } while (0)

#define da_free(xp)                                                             \
    do {                                                                        \
        rl_free((xp)->items,                                                    \
                sizeof(*(xp)->items) * (xp)->capacity,                          \
                MEM_DYNAMIC_ARRAY);                                             \
        (xp)->capacity = 0;                                                     \
        (xp)->count = 0;                                                        \
        (xp)->items = nullptr;                                                  \
    } while (0)