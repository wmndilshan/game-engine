#pragma once

#include <cstdint>

namespace engine {

/// Off-screen Framebuffer Object for rendering the 3D scene into an ImGui viewport.
/// Has two colour attachments:
///   0 — RGBA scene colour
///   1 — R32I entity-ID (for mouse picking)
class Framebuffer
{
public:
    Framebuffer()  = default;
    ~Framebuffer() = default;

    Framebuffer(const Framebuffer&)            = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    /// Create the FBO, colour texture, entity-ID texture, and depth/stencil RBO.
    /// Returns false if the framebuffer is incomplete.
    bool init(int width, int height);

    /// Bind this FBO — all subsequent GL draw calls render into it.
    void bind();

    /// Unbind the FBO — restores the default (screen) framebuffer.
    void unbind();

    /// Resize all attachments.  Call when the ImGui viewport region changes size.
    void rescale(int width, int height);

    /// Clear the entity-ID attachment to -1 (no entity).  Call after bind().
    void clearEntityIDs();

    /// Read a single pixel from the entity-ID attachment.
    /// Coordinates are in FBO space (origin bottom-left).
    int readPixel(std::uint32_t x, std::uint32_t y);

    /// Release all GPU resources.
    void shutdown();

    /// Get the colour-attachment texture ID (for ImGui::Image).
    unsigned int getTexture() const { return m_texture; }

    int getWidth()  const { return m_width; }
    int getHeight() const { return m_height; }

private:
    unsigned int m_FBO             = 0;
    unsigned int m_texture         = 0;   // colour attachment 0
    unsigned int m_entityIDTexture = 0;   // colour attachment 1 (R32I)
    unsigned int m_RBO             = 0;

    int m_width  = 0;
    int m_height = 0;
};

} // namespace engine
