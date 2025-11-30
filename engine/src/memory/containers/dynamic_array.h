#pragma once

#include "defines.h"
#include "memory/memory.h"

// Define the structure
#define DA_DEFINE(name, type)                   \
    typedef struct {                            \
        type *items;                            \
        u64 count;                              \
        u64 capacity;                           \
    } name

#define RESIZE_FACTOR 2

DA_DEFINE(U16, u16);

#define DA_APPEND(xp, x)                                                          \
    do {                                                                          \
        if ((xp)->count >= (xp)->capacity) {                                      \
            u64 old_cap = (xp)->capacity;                                         \
            if (old_cap == 0) old_cap = 10;                                       \
            u64 new_cap = old_cap * RESIZE_FACTOR;                                \
                                                                                  \
            void *new_arr = rl_alloc(new_cap * sizeof(*(xp)->items),              \
                                     MEM_DYNAMIC_ARRAY);                          \
                                                                                  \
            rl_copy((xp)->items, new_arr, old_cap * sizeof(*(xp)->items));        \
            rl_free((xp)->items, old_cap * sizeof(*(xp)->items),                  \
                     MEM_DYNAMIC_ARRAY);                                          \
                                                                                  \
            (xp)->items = new_arr;                                                \
            (xp)->capacity = new_cap;                                             \
        }                                                                         \
        (xp)->items[(xp)->count++] = (x);                                         \
    } while (0)