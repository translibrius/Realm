#pragma once

#include "defines.h"
#include "../vendor/cglm/cglm.h"

typedef struct GL_Shader {
    i32 program_id;
    i32 vertex_id;
    i32 fragment_id;
} GL_Shader;

b8 opengl_shader_setup(const char *vertex, const char *frag, GL_Shader *out_shader);
void opengl_shader_use(GL_Shader *shader);

void opengl_shader_set_bool(GL_Shader *shader, const char *name, b8 value);
void opengl_shader_set_i32(GL_Shader *shader, const char *name, i32 value);
void opengl_shader_set_f32(GL_Shader *shader, const char *name, f32 value);
void opengl_shader_set_vec2(GL_Shader *shader, const char *name, vec2 value);
void opengl_shader_set_vec3(GL_Shader *shader, const char *name, vec3 value);
void opengl_shader_set_vec4(GL_Shader *shader, const char *name, vec4 value);