#include "gl_mesh.h"
#include "renderer/mesh_data.h"

void gl_mesh_destroy(GL_Mesh *mesh) {
    if (mesh->ebo) {
        glDeleteBuffers(1, &mesh->ebo);
    }
    glDeleteBuffers(1, &mesh->vbo);
    glDeleteVertexArrays(1, &mesh->vao);
}

void gl_mesh_draw(GL_Mesh *mesh) {
    glBindVertexArray(mesh->vao);
    if (mesh->index_count > 0) {
        glDrawElements(GL_TRIANGLES, mesh->index_count, GL_UNSIGNED_INT, 0);
    } else {
        glDrawArrays(GL_TRIANGLES, 0, mesh->vertex_count);
    }
}

static void gl_mesh_setup_attribs(void) {
    // position — layout(location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, pos));
    glEnableVertexAttribArray(0);

    // normal — layout(location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, normal));
    glEnableVertexAttribArray(1);

    // texture coord — layout(location = 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, tex_coord));
    glEnableVertexAttribArray(2);
}

GL_Mesh gl_mesh_create_cube(void) {
    u32 vbo, vao;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * cube_vertex_count, cube_vertices, GL_STATIC_DRAW);

    gl_mesh_setup_attribs();

    return (GL_Mesh){.vao = vao, .vbo = vbo, .vertex_count = cube_vertex_count};
}

GL_Mesh gl_mesh_create_from_primitive(const vertex *vertices, u32 vertex_count,
                                       const u32 *indices, u32 index_count) {
    u32 vbo, vao, ebo = 0;

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * vertex_count, vertices, GL_STATIC_DRAW);

    if (indices && index_count > 0) {
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(u32) * index_count, indices, GL_STATIC_DRAW);
    }

    gl_mesh_setup_attribs();

    return (GL_Mesh){
        .vao = vao, .vbo = vbo, .ebo = ebo,
        .vertex_count = vertex_count, .index_count = index_count,
    };
}
