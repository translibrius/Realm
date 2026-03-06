#include "gl_mesh.h"
#include "renderer/mesh_data.h"

void gl_mesh_destroy(GL_Mesh *mesh) {
    glDeleteBuffers(1, &mesh->vbo);
    glDeleteVertexArrays(1, &mesh->vao);
    mesh = nullptr;
}

void gl_mesh_draw(GL_Mesh *mesh) {
    glBindVertexArray(mesh->vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh->vertex_count);
}

GL_Mesh gl_mesh_create_cube() {
    u32 vbo, vao;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * cube_vertex_count, cube_vertices, GL_STATIC_DRAW);

    // position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, pos));
    glEnableVertexAttribArray(0);

    // normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, normal));
    glEnableVertexAttribArray(1);

    // texture coord
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, tex_coord));
    glEnableVertexAttribArray(2);

    return (GL_Mesh){.vao = vao, .vbo = vbo, .vertex_count = cube_vertex_count};
}
