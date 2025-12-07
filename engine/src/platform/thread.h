#pragma once

#include "defines.h"

typedef struct rl_thread {
    u64 id;
    void *handle; // OS Handle
    void (*entry)();
} rl_thread;

b8 platform_thread_create(void (*entry)(), rl_thread *out_thread);