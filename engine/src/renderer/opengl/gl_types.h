#pragma once

#include "defines.h"
#include "gl_shader.h"
#include "gl_texture.h"
#include "memory/arena.h"
#include "memory/containers/dynamic_array.h"
#include "platform/platform.h"
#include "gl_mesh.h"
#include "asset/font.h"
#include "core/camera.h"

typedef struct {
    u32 texture_id;
    rl_font *font;
} GL_Font;

DA_DEFINE(GL_Fonts, GL_Font);

typedef struct GL_TextPipeline {
    u32 vao;
    u32 vbo;
    GL_Shader shader;
} GL_TextPipeline;

typedef struct GL_TextVertex {
    vec2 pos;
    vec2 uv;
} GL_TextVertex;

typedef struct GL_Context {
    platform_window *window;
    rl_arena arena;
    rl_camera *camera;

    // Text
    GL_TextPipeline text_pipeline;
    GL_Fonts fonts;
    rl_font *active_font;

    // Defaults
    GL_Shader default_shader;
    GL_Shader light_shader;
    GL_Texture wood_texture;
    GL_Mesh cube_mesh;

    // Mat
    mat4 view;
    mat4 projection;
} GL_Context;