#pragma once

#include "defines.h"
#include "cglm.h"

#define GL_SHADER_MAX_UNIFORMS 32

typedef struct GL_UniformEntry {
    const char *name;
    i32 location;
} GL_UniformEntry;

typedef struct GL_Shader {
    i32 program_id;
    i32 vertex_id;
    i32 fragment_id;
    GL_UniformEntry uniforms[GL_SHADER_MAX_UNIFORMS];
    u32 uniform_count;
} GL_Shader;

b8 opengl_shader_setup(u32 vertex_asset_id, u32 frag_asset_id, GL_Shader *out_shader);
void opengl_shader_use(GL_Shader *shader);

void opengl_shader_set_bool(GL_Shader *shader, const char *name, b8 value);
void opengl_shader_set_i32(GL_Shader *shader, const char *name, i32 value);
void opengl_shader_set_f32(GL_Shader *shader, const char *name, f32 value);
void opengl_shader_set_vec2(GL_Shader *shader, const char *name, vec2 value);
void opengl_shader_set_vec3(GL_Shader *shader, const char *name, vec3 value);
void opengl_shader_set_vec4(GL_Shader *shader, const char *name, vec4 value);
void opengl_shader_set_mat4(GL_Shader *shader, const char *name, mat4 value);
