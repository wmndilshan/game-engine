#include "Framebuffer.h"

#include <glad/glad.h>
#include <cstdio>

namespace engine {

bool Framebuffer::init(int width, int height)
{
    m_width  = width;
    m_height = height;

    // -- Create FBO -----------------------------------------------------------
    glGenFramebuffers(1, &m_FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // -- Colour attachment 0 (RGBA scene colour) ------------------------------
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 m_width, m_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, m_texture, 0);

    // -- Colour attachment 1 (R32I entity ID for mouse picking) ---------------
    glGenTextures(1, &m_entityIDTexture);
    glBindTexture(GL_TEXTURE_2D, m_entityIDTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I,
                 m_width, m_height, 0,
                 GL_RED_INTEGER, GL_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1,
                           GL_TEXTURE_2D, m_entityIDTexture, 0);

    // -- Tell OpenGL to draw into both attachments ----------------------------
    GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    // -- Depth + stencil attachment (renderbuffer) ----------------------------
    glGenRenderbuffers(1, &m_RBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,
                              GL_RENDERBUFFER, m_RBO);

    // -- Completeness check ---------------------------------------------------
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::fprintf(stderr, "[Framebuffer] ERROR: Framebuffer is not complete!\n");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    std::printf("[Framebuffer] Created %dx%d FBO (id %u) with entity-ID attachment\n",
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

void Framebuffer::clearEntityIDs()
{
    // Clear the entity-ID attachment (index 1) to -1 using glClearBufferiv.
    // This is available in OpenGL 3.0+ (unlike glClearTexImage which needs 4.4).
    const GLint clearValue = -1;
    glClearBufferiv(GL_COLOR, 1, &clearValue);
}

int Framebuffer::readPixel(std::uint32_t x, std::uint32_t y)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);
    glReadBuffer(GL_COLOR_ATTACHMENT1);

    int pixelData = -1;
    glReadPixels(static_cast<GLint>(x), static_cast<GLint>(y),
                 1, 1,
                 GL_RED_INTEGER, GL_INT, &pixelData);

    return pixelData;
}

void Framebuffer::rescale(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    m_width  = width;
    m_height = height;

    // Bind the FBO so glDrawBuffers applies to it
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO);

    // Re-create colour texture at new size
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                 m_width, m_height, 0,
                 GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    // Re-create entity-ID texture at new size
    glBindTexture(GL_TEXTURE_2D, m_entityIDTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32I,
                 m_width, m_height, 0,
                 GL_RED_INTEGER, GL_INT, nullptr);

    // Re-create depth/stencil RBO at new size
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
                          m_width, m_height);

    // Restore draw-buffers state (needed after rescale)
    GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::shutdown()
{
    if (m_FBO)             { glDeleteFramebuffers(1, &m_FBO);            m_FBO = 0;             }
    if (m_texture)         { glDeleteTextures(1, &m_texture);            m_texture = 0;         }
    if (m_entityIDTexture) { glDeleteTextures(1, &m_entityIDTexture);    m_entityIDTexture = 0; }
    if (m_RBO)             { glDeleteRenderbuffers(1, &m_RBO);           m_RBO = 0;             }
}

} // namespace engine
