#pragma once

#include "renderer_types.h"

// Shared cube geometry — consistent CCW winding viewed from outside.
// Both OpenGL and Vulkan backends reference this directly.
extern const vertex cube_vertices[36];
extern const u32 cube_vertex_count;
