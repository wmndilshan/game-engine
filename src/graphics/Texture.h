#pragma once

#include <string>
#include <cstdint>

namespace engine {

class Texture
{
public:
    Texture()  = default;
    ~Texture() = default;

    Texture(const Texture&)            = delete;
    Texture& operator=(const Texture&) = delete;

    /// Load an image from disk and upload it to the GPU.
    /// Returns false on failure.
    bool init(const std::string& path);

    /// Bind the texture to the given texture unit (0–15).
    void bind(std::uint32_t slot = 0) const;

    /// Unbind the texture unit.
    void unbind() const;

    /// Delete the GPU texture object.
    void shutdown();

    int getWidth()  const { return m_Width; }
    int getHeight() const { return m_Height; }

private:
    unsigned int m_RendererID = 0;
    int          m_Width      = 0;
    int          m_Height     = 0;
    int          m_BPP        = 0;   // bytes-per-pixel / channels
    std::string  m_FilePath;
};

} // namespace engine
