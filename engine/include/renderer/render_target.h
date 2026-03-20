#pragma once

#include "defines.h"

typedef enum rl_rt_format {
    RL_RT_FORMAT_RGBA8,   // mask buffer
    RL_RT_FORMAT_RGBA16F, // JFA coordinate buffers
} rl_rt_format;

// Opaque — backend-defined
typedef struct rl_render_target rl_render_target;
