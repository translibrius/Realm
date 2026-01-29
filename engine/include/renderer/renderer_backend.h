#pragma once

#include "defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum RENDERER_BACKEND {
    BACKEND_OPENGL,
    BACKEND_VULKAN
} RENDERER_BACKEND;

#ifdef __cplusplus
}
#endif
