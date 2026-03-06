#pragma once

#include <glm/glm.hpp>

#include <vector>
#include <cstdint>

namespace engine {

/// Per-vertex data layout matching the shader attributes.
struct Vertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

/// A single renderable mesh — owns its own VAO/VBO/EBO on the GPU.
class Mesh
{
public:
    std::vector<Vertex>       vertices;
    std::vector<std::uint32_t> indices;

    Mesh(std::vector<Vertex> vertices, std::vector<std::uint32_t> indices);
    ~Mesh() = default;

    Mesh(const Mesh&)            = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Allow move so we can store meshes in a vector
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    /// Bind VAO and issue the indexed draw call.
    void draw() const;

    /// Release GPU resources.
    void shutdown();

private:
    unsigned int m_VAO = 0;
    unsigned int m_VBO = 0;
    unsigned int m_EBO = 0;

    /// Upload vertex/index data to the GPU and configure attribute pointers.
    void setupMesh();
};

} // namespace engine
