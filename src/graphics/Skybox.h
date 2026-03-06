#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace engine {

/// Cubemap skybox rendered at maximum depth so it always appears behind the scene.
class Skybox
{
public:
    Skybox()  = default;
    ~Skybox() = default;

    Skybox(const Skybox&)            = delete;
    Skybox& operator=(const Skybox&) = delete;

    /// Load six face images and compile the skybox shader.
    /// Order: +X, −X, +Y, −Y, +Z, −Z.
    bool init(const std::vector<std::string>& faces);

    /// Draw the skybox.  Call AFTER the rest of the scene has been rendered.
    void draw(const glm::mat4& view, const glm::mat4& projection);

    /// Release all GPU resources.
    void shutdown();

private:
    unsigned int m_VAO            = 0;
    unsigned int m_VBO            = 0;
    unsigned int m_cubemapTexture = 0;
    unsigned int m_shaderProgram  = 0;
};

} // namespace engine
