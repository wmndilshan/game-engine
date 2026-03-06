#include "Framebuffer.h"

#include <glad/glad.h>
#include <cstdio>

namespace engine {

bool Framebuffer::init(int width, int height)
{
    m_width  = width;
    m_height = height;

    // ── Create FBO ──────────────────────────────────────────────────────
    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // ── Colour attachment (texture) ─────────────────────────────────────
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 m_width, m_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_texture, 0);

    // ── Depth + stencil attachment (renderbuffer) ───────────────────────
    glGenRenderbuffers(1, &m_RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_RBO);

    // ── Completeness check ──────────────────────────────────────────────
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::fprintf(stderr, "[Framebuffer] ERROR: Framebuffer is not complete!\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::printf("[Framebuffer] Created %dx%d FBO (id %u)\n",
                m_width, m_height, m_FBO);
    return true;
}

void Framebuffer::bind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glViewport(0, 0, m_width, m_height);
}

void Framebuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::rescale(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    m_width  = width;
    m_height = height;

    // Re-create colour texture at new size
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 m_width, m_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // Re-create depth/stencil RBO at new size
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          m_width, m_height);
}

void Framebuffer::shutdown()
{
    if (m_FBO)     { glDeleteFramebuffers(1, &m_FBO);    m_FBO = 0;     }
    if (m_texture) { glDeleteTextures(1, &m_texture);    m_texture = 0; }
    if (m_RBO)     { glDeleteRenderbuffers(1, &m_RBO);   m_RBO = 0;     }
}

} // namespace engine
