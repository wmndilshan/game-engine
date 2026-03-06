#include "Mesh.h"

#include <glad/glad.h>

#include <utility>  // std::move, std::exchange

namespace engine {

Mesh::Mesh(std::vector<Vertex> verts, std::vector<std::uint32_t> inds)
    : vertices(std::move(verts))
    , indices(std::move(inds))
{
    setupMesh();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vertices(std::move(other.vertices))
    , indices(std::move(other.indices))
    , m_VAO(std::exchange(other.m_VAO, 0))
    , m_VBO(std::exchange(other.m_VBO, 0))
    , m_EBO(std::exchange(other.m_EBO, 0))
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        shutdown();
        vertices = std::move(other.vertices);
        indices  = std::move(other.indices);
        m_VAO    = std::exchange(other.m_VAO, 0);
        m_VBO    = std::exchange(other.m_VBO, 0);
        m_EBO    = std::exchange(other.m_EBO, 0);
    }
    return *this;
}

void Mesh::setupMesh()
{
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                 vertices.data(),
                 GL_STATIC_DRAW);

    // Index buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                 indices.data(),
                 GL_STATIC_DRAW);

    // Attribute 0: Position  (vec3 — offset 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, Position)));
    glEnableVertexAttribArray(0);

    // Attribute 1: Normal    (vec3 — offset 12)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, Normal)));
    glEnableVertexAttribArray(1);

    // Attribute 2: TexCoords (vec2 — offset 24)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void*>(offsetof(Vertex, TexCoords)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Mesh::draw() const
{
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES,
                   static_cast<GLsizei>(indices.size()),
                   GL_UNSIGNED_INT,
                   nullptr);
    glBindVertexArray(0);
}

void Mesh::shutdown()
{
    if (m_EBO) { glDeleteBuffers(1, &m_EBO); m_EBO = 0; }
    if (m_VBO) { glDeleteBuffers(1, &m_VBO); m_VBO = 0; }
    if (m_VAO) { glDeleteVertexArrays(1, &m_VAO); m_VAO = 0; }
}

} // namespace engine
