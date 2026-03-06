#pragma once

namespace engine {

/// Off-screen Framebuffer Object for rendering the 3D scene into an ImGui viewport.
class Framebuffer
{
public:
    Framebuffer()  = default;
    ~Framebuffer() = default;

    Framebuffer(const Framebuffer&)            = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;

    /// Create the FBO, colour texture, and depth/stencil RBO.
    /// Returns false if the framebuffer is incomplete.
    bool init(int width, int height);

    /// Bind this FBO — all subsequent GL draw calls render into it.
    void bind();

    /// Unbind the FBO — restores the default (screen) framebuffer.
    void unbind();

    /// Resize the colour texture and depth/stencil RBO.
    /// Call when the ImGui viewport region changes size.
    void rescale(int width, int height);

    /// Release all GPU resources.
    void shutdown();

    /// Get the colour‐attachment texture ID (for ImGui::Image).
    unsigned int getTexture() const { return m_texture; }

    int getWidth()  const { return m_width; }
    int getHeight() const { return m_height; }

private:
    unsigned int m_FBO     = 0;
    unsigned int m_texture = 0;
    unsigned int m_RBO     = 0;

    int m_width  = 0;
    int m_height = 0;
};

} // namespace engine
