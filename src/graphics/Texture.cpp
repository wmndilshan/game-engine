// ── stb_image implementation — MUST appear in exactly ONE .cpp ──────────────
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"

#include <glad/glad.h>

#include "../core/Log.h"

namespace engine {

bool Texture::init(const std::string& path)
{
    m_FilePath = path;

    // stb loads images with (0,0) at top-left; OpenGL expects bottom-left.
    stbi_set_flip_vertically_on_load(1);

    unsigned char* data = stbi_load(path.c_str(), &m_Width, &m_Height, &m_BPP, 0);
    if (!data)
    {
        GE_WARN(std::string("[Texture] Failed to load: ") + path);
        return false;
    }

    GLenum internalFormat = GL_RGB;
    GLenum dataFormat     = GL_RGB;

    if (m_BPP == 4)
    {
        internalFormat = GL_RGBA;
        dataFormat     = GL_RGBA;
    }
    else if (m_BPP == 1)
    {
        internalFormat = GL_RED;
        dataFormat     = GL_RED;
    }

    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);

    // Wrapping
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Upload
    glTexImage2D(GL_TEXTURE_2D, 0,
                 static_cast<GLint>(internalFormat),
                 m_Width, m_Height, 0,
                 dataFormat, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    GE_INFO(std::string("[Texture] Loaded ") + path + " (" + std::to_string(m_Width) + "x" + std::to_string(m_Height) + ", " + std::to_string(m_BPP) + " channels)");
    return true;
}

void Texture::bind(std::uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
}

void Texture::unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::shutdown()
{
    if (m_RendererID)
    {
        glDeleteTextures(1, &m_RendererID);
        m_RendererID = 0;
    }
}

} // namespace engine
