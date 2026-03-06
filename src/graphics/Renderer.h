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

    /// Release all GPU resources.
    void shutdown();

private:
    unsigned int m_shaderProgram = 0;
    unsigned int m_VAO           = 0;
    unsigned int m_VBO           = 0;

    int m_locModel      = -1;
    int m_locView       = -1;
    int m_locProjection = -1;
};

} // namespace engine
