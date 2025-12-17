#include "renderer/opengl/gl_texture.h"

#include "asset/asset.h"
#include "asset/texture.h"
#include "vendor/glad/glad.h"

b8 opengl_texture_generate(const char *filename, GL_Texture *out_texture) {
    rl_asset *asset = get_asset(filename);
    rl_texture *texture = asset->handle;

    u32 texture_id;
    glGenTextures(1, &texture_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_id);

    // Set the texture wrapping/filtering options (on currently bound texture obj)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texture->width, texture->height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture->data);
    glGenerateMipmap(GL_TEXTURE_2D);

    out_texture->id = texture_id;

    // NOTE: Potentially free the asset, since its already generated a gl texture
    // But we might want to keep the texture asset loaded in case we want to generate the texture again ?
    // However, currently asset system uses arena allocator, so i can't free specific assets :/

    return true;
}