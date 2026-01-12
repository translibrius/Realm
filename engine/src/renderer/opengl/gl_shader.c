#include "renderer/opengl/gl_shader.h"

#include "gl_renderer.h"
#include "asset/shader.h"
#include "glad.h"

// Forward decl.
b8 opengl_compile_vertex_shader(const char *source, i32 *out_id);
b8 opengl_compile_fragment_shader(const char *source, i32 *out_id);
b8 opengl_create_shader_program(i32 vertex_id, i32 fragment_id, i32 *out_prog_id);

// TODO: Add cache of compiled shaders to reuse
b8 opengl_shader_setup(const char *vertex, const char *frag, GL_Shader *out_shader) {
    rl_asset_shader *default_vert = get_asset(vertex)->handle;
    rl_asset_shader *default_frag = get_asset(frag)->handle;

    i32 vert_id, frag_id, program_id;
    if (!opengl_compile_vertex_shader(default_vert->source, &vert_id)) {
        return false;
    }

    if (!opengl_compile_fragment_shader(default_frag->source, &frag_id)) {
        return false;
    }

    if (!opengl_create_shader_program(vert_id, frag_id, &program_id)) {
        return false;
    }

    out_shader->fragment_id = frag_id;
    out_shader->vertex_id = vert_id;
    out_shader->program_id = program_id;

    return true;
}

void opengl_shader_use(GL_Shader *shader) {
    glUseProgram(shader->program_id);
}

void opengl_shader_set_bool(GL_Shader *shader, const char *name, b8 value) {
    glUniform1i(glGetUniformLocation(shader->program_id, name), value);
}

void opengl_shader_set_i32(GL_Shader *shader, const char *name, i32 value) {
    glUniform1i(glGetUniformLocation(shader->program_id, name), value);
}

void opengl_shader_set_f32(GL_Shader *shader, const char *name, f32 value) {
    glUniform1f(glGetUniformLocation(shader->program_id, name), value);
}

void opengl_shader_set_vec2(GL_Shader *shader, const char *name, vec2 value) {
    i32 loc = glGetUniformLocation(shader->program_id, name);
    glUniform2f(loc, value[0], value[1]);
}

void opengl_shader_set_vec3(GL_Shader *shader, const char *name, vec3 value) {
    i32 loc = glGetUniformLocation(shader->program_id, name);
    glUniform3f(loc, value[0], value[1], value[2]);
}

void opengl_shader_set_vec4(GL_Shader *shader, const char *name, vec4 value) {
    i32 loc = glGetUniformLocation(shader->program_id, name);
    glUniform4f(loc, value[0], value[1], value[2], value[3]);
}

void opengl_shader_set_mat4(GL_Shader *shader, const char *name, mat4 value) {
    i32 loc = glGetUniformLocation(shader->program_id, name);
    glUniformMatrix4fv(loc, 1, GL_FALSE, value);
}

b8 opengl_create_shader_program(i32 vertex_id, i32 fragment_id, i32 *out_prog_id) {
    // Create shader program
    i32 program_id = glCreateProgram();
    glAttachShader(program_id, vertex_id);
    glAttachShader(program_id, fragment_id);
    glLinkProgram(program_id);

    // Delete after linking to program
    glDeleteShader(vertex_id);
    glDeleteShader(fragment_id);

    i32 success;
    char info_log[512];
    glGetProgramiv(program_id, GL_LINK_STATUS, &success);

    if (!success) {
        glGetProgramInfoLog(program_id, 512, nullptr, info_log);
        RL_ERROR("Failed to link shader program with shaders: %s", info_log);
        return false;
    }

    *out_prog_id = program_id;

    return true;
}

b8 opengl_compile_vertex_shader(const char *source, i32 *out_id) {
    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, &source, nullptr);
    glCompileShader(vertex_shader);

    i32 success;
    char infoLog[512];
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(vertex_shader, 512, nullptr, infoLog);
        RL_ERROR("Failed to compile vertex shader: %s", infoLog);
        return false;
    }

    *out_id = vertex_shader;

    return vertex_shader;
}

b8 opengl_compile_fragment_shader(const char *source, i32 *out_id) {
    i32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, &source, nullptr);
    glCompileShader(fragment_shader);

    i32 success;
    char infoLog[512];
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        glGetShaderInfoLog(fragment_shader, 512, nullptr, infoLog);
        RL_ERROR("Failed to compile fragment shader: %s", infoLog);
        return false;
    }

    *out_id = fragment_shader;

    return true;
}