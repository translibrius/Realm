#pragma once

#include "defines.h"
#include "math_types.h"

RL_INLINE vec2 vec2_create(f32 x, f32 y) {
    return (vec2){x, y};
}

RL_INLINE vec3 vec3_create(f32 x, f32 y, f32 z) {
    return (vec3){x, y, z};
}