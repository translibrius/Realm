#pragma once

#include "defines.h"
#include "glad.h"

typedef struct GL_Mesh {
    u32 vao;
    u32 vbo;
    u32 vertex_count;
} GL_Mesh;

void gl_mesh_destroy(GL_Mesh *mesh);
void gl_mesh_draw(GL_Mesh *mesh);

GL_Mesh gl_mesh_create_cube(void);