#pragma once

#include "defines.h"
#include "glad.h"
#include "renderer/renderer_types.h"

typedef struct GL_Mesh {
    u32 vao;
    u32 vbo;
    u32 ebo;           // 0 if non-indexed
    u32 vertex_count;
    u32 index_count;   // 0 if non-indexed
} GL_Mesh;

void gl_mesh_destroy(GL_Mesh *mesh);
void gl_mesh_draw(GL_Mesh *mesh);

GL_Mesh gl_mesh_create_cube(void);
GL_Mesh gl_mesh_create_from_primitive(const vertex *vertices, u32 vertex_count,
                                       const u32 *indices, u32 index_count);