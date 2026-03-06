#pragma once

#include <glm/glm.hpp>

namespace engine {

class Renderer
{
public:
    Renderer()  = default;
    ~Renderer() = default;

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;

    /// Compile shaders, create VAO/VBO for the built-in cube.
    bool init();

    /// Clear the framebuffer with the given RGBA colour.
    void clear(const glm::vec4& color);

    /// Draw the built-in cube with the given MVP matrices.
    void drawCube(const glm::mat4& model,
                  const glm::mat4& view,
                  const glm::mat4& projection);

    /// Bind the shader and set view/projection. Call once per frame before drawing models.
    void beginScene(const glm::mat4& view, const glm::mat4& projection);

    /// Set the model matrix for the next draw call.
    void setModelMatrix(const glm::mat4& model);

    /// Set whether the next draw call should use a bound texture (1) or flat colour (0).
    void setHasTexture(bool has);

    /// Release all GPU resources.
    void shutdown();

    /// Get the raw shader program so external code can set extra uniforms if needed.
    unsigned int getShaderProgram() const { return m_shaderProgram; }

private:
    unsigned int m_shaderProgram = 0;
    unsigned int m_VAO           = 0;
    unsigned int m_VBO           = 0;

    int m_locModel       = -1;
    int m_locView        = -1;
    int m_locProjection  = -1;
    int m_locHasTexture  = -1;
    int m_locLightDir    = -1;
    int m_locLightColor  = -1;
    int m_locObjectColor = -1;
};

} // namespace engine
