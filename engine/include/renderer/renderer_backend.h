#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
    BACKEND_VULKAN
} RENDERER_BACKEND;

#define RL_CLEAR_COLOR_R 0.2f
#define RL_CLEAR_COLOR_G 0.2f
#define RL_CLEAR_COLOR_B 0.2f
#define RL_CLEAR_COLOR_A 1.0f

#ifdef __cplusplus
}
#endif
