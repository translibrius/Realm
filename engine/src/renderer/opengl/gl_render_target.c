#include "gl_render_target.h"
#include "core/logger.h"

static GLenum rt_internal_format(rl_rt_format format) {
    switch (format) {
    case RL_RT_FORMAT_RGBA8:   return GL_RGBA8;
    case RL_RT_FORMAT_RGBA16F: return GL_RGBA16F;
    }
    return GL_RGBA8;
}

static GLenum rt_data_type(rl_rt_format format) {
    switch (format) {
    case RL_RT_FORMAT_RGBA8:   return GL_UNSIGNED_BYTE;
    case RL_RT_FORMAT_RGBA16F: return GL_FLOAT;
    }
    return GL_UNSIGNED_BYTE;
}

b8 gl_render_target_create(GL_RenderTarget *rt, u32 width, u32 height, rl_rt_format format, b8 with_depth) {
    rt->width = width;
    rt->height = height;
    rt->format = format;

    // Color texture
    glGenTextures(1, &rt->color_texture);
    glBindTexture(GL_TEXTURE_2D, rt->color_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, rt_internal_format(format), width, height, 0, GL_RGBA, rt_data_type(format), nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // FBO
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color_texture, 0);

    // Optional depth renderbuffer
    if (with_depth) {
        glGenRenderbuffers(1, &rt->depth_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rt->depth_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depth_rbo);
    } else {
        rt->depth_rbo = 0;
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        RL_ERROR("GL render target incomplete: 0x%x", status);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void gl_render_target_destroy(GL_RenderTarget *rt) {
    if (rt->fbo)           { glDeleteFramebuffers(1, &rt->fbo);           rt->fbo = 0; }
    if (rt->color_texture) { glDeleteTextures(1, &rt->color_texture);     rt->color_texture = 0; }
    if (rt->depth_rbo)     { glDeleteRenderbuffers(1, &rt->depth_rbo);    rt->depth_rbo = 0; }
}

void gl_render_target_begin(GL_RenderTarget *rt, f32 r, f32 g, f32 b, f32 a) {
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    glViewport(0, 0, rt->width, rt->height);
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT | (rt->depth_rbo ? GL_DEPTH_BUFFER_BIT : 0));
}

void gl_render_target_end(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void gl_render_target_bind_texture(GL_RenderTarget *rt, u32 slot) {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, rt->color_texture);
}

b8 gl_render_target_resize(GL_RenderTarget *rt, u32 width, u32 height) {
    if (rt->width == width && rt->height == height) return true;

    rl_rt_format format = rt->format;
    b8 has_depth = (rt->depth_rbo != 0);
    gl_render_target_destroy(rt);
    return gl_render_target_create(rt, width, height, format, has_depth);
}
