#pragma once

#include "defines.h"

typedef struct GL_Shader {
    i32 program_id;
    i32 vertex_id;
    i32 fragment_id;
} GL_Shader;

b8 opengl_shader_setup(const char *vertex, const char *frag, GL_Shader *out_shader);