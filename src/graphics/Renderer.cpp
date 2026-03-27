#include "Renderer.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <string>

#include "../core/Log.h"

namespace engine {

// ── Shader sources (GLSL 1.40 — compatible with OpenGL 3.1+) ───────────────
// These shaders support Position (attrib 0), Normal (attrib 1), TexCoords (attrib 2).
// A simple directional light + ambient provides basic shading for loaded models.

static const char* vertexShaderSource = R"(
#version 140

in vec3 aPos;
in vec3 aNormal;
in vec2 aTexCoord;

out vec2 TexCoord;
out vec3 FragNormal;
out vec3 FragPos;

uniform mat4 model;
uniform mat3 normalMatrix;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos   = worldPos.xyz;
    FragNormal = normalMatrix * aNormal;
    TexCoord   = aTexCoord;
    gl_Position = projection * view * worldPos;
}
)";

static const char* fragmentShaderSource = R"(
#version 140

in vec2 TexCoord;
in vec3 FragNormal;
in vec3 FragPos;

out vec4 color0;    // colour attachment 0 — scene colour
out int  color1;    // colour attachment 1 — entity ID (R32I)

uniform sampler2D u_Texture;
uniform float u_HasTexture;     // 1.0 = textured, 0.0 = use flat colour
uniform vec3  u_LightDir;       // direction TO the light (normalised)
uniform vec3  u_LightColor;
uniform vec3  u_ObjectColor;    // fallback when there is no texture
uniform int   u_EntityID;       // ECS entity ID for mouse picking

void main()
{
    // Normalise interpolated normal
    vec3 norm = normalize(FragNormal);

    // Ambient
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * u_LightColor;

    // Diffuse
    float diff = max(dot(norm, u_LightDir), 0.0);
    vec3 diffuse = diff * u_LightColor;

    // Surface colour — blend between flat colour and texture via u_HasTexture
    vec3 texColor  = texture(u_Texture, TexCoord).rgb;
    vec3 surfaceColor = mix(u_ObjectColor, texColor, u_HasTexture);

    vec3 result = (ambient + diffuse) * surfaceColor;
    color0 = vec4(result, 1.0);
    color1 = u_EntityID;
}
)";

// ── Cube vertex data (position XYZ + normal XYZ + UV) ──────────────────────
// 36 vertices — 6 faces × 2 triangles × 3 verts, no index buffer needed.
// Stride: 8 floats per vertex (3 pos + 3 normal + 2 uv).

// clang-format off
static const float cubeVertices[] = {
    // positions          // normals           // UVs
    // Front face (+Z)
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  0.0f, 0.0f,

    // Back face (-Z)
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,

    // Left face (-X)
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,

    // Right face (+X)
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,

    // Top face (+Y)
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,

    // Bottom face (-Y)
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
};
// clang-format on

// ── Internal helpers ────────────────────────────────────────────────────────

static GLuint compileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        GE_ERROR(std::string("[Renderer] Shader compile error: ") + info);
    }
    return shader;
}

static GLuint linkProgram(GLuint vert, GLuint frag)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);

    // GLSL 1.40 has no layout qualifiers — bind locations manually
    glBindAttribLocation(program, 0, "aPos");
    glBindAttribLocation(program, 1, "aNormal");
    glBindAttribLocation(program, 2, "aTexCoord");

    // Bind fragment data locations for MRT (colour attachment 0 & 1)
    glBindFragDataLocation(program, 0, "color0");
    glBindFragDataLocation(program, 1, "color1");

    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char info[512];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        GE_ERROR(std::string("[Renderer] Program link error: ") + info);
    }
    return program;
}

// ── Public API ──────────────────────────────────────────────────────────────

bool Renderer::init()
{
    // Compile & link shaders
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    m_shaderProgram = linkProgram(vert, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    if (m_shaderProgram == 0)
        return false;

    // Cache uniform locations
    m_locModel      = glGetUniformLocation(m_shaderProgram, "model");
    m_locNormalMatrix = glGetUniformLocation(m_shaderProgram, "normalMatrix");
    m_locView       = glGetUniformLocation(m_shaderProgram, "view");
    m_locProjection = glGetUniformLocation(m_shaderProgram, "projection");

    // Lighting & material uniforms
    m_locHasTexture  = glGetUniformLocation(m_shaderProgram, "u_HasTexture");
    m_locLightDir    = glGetUniformLocation(m_shaderProgram, "u_LightDir");
    m_locLightColor  = glGetUniformLocation(m_shaderProgram, "u_LightColor");
    m_locObjectColor = glGetUniformLocation(m_shaderProgram, "u_ObjectColor");
    m_locEntityID    = glGetUniformLocation(m_shaderProgram, "u_EntityID");

    // Set the texture sampler to unit 0 (only needs to be done once)
    glUseProgram(m_shaderProgram);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "u_Texture"), 0);
    glUniform1i(m_locEntityID, -1);  // default: no entity

    // Default light direction (from upper-right-front)
    glm::vec3 lightDir = glm::normalize(glm::vec3(0.5f, 1.0f, 0.8f));
    glUniform3f(m_locLightDir, lightDir.x, lightDir.y, lightDir.z);
    glUniform3f(m_locLightColor, 1.0f, 1.0f, 1.0f);
    glUniform3f(m_locObjectColor, 0.8f, 0.5f, 0.3f); // warm orange fallback
    glUniform1f(m_locHasTexture, 0.0f);                // no texture by default
    glUseProgram(0);

    // VAO / VBO
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    // position  (location = 0)  —  3 floats, stride = 8 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    // normal    (location = 1)  —  3 floats, offset = 3 floats
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // texcoord  (location = 2)  —  2 floats, offset = 6 floats
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float),
                          reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // Global GL state
    glEnable(GL_DEPTH_TEST);

    return true;
}

void Renderer::clear(const glm::vec4& color)
{
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::drawCube(const glm::mat4& model,
                        const glm::mat4& view,
                        const glm::mat4& projection,
                        int entityID)
{
    glUseProgram(m_shaderProgram);

    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));

    glUniformMatrix4fv(m_locModel,      1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(m_locNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    glUniformMatrix4fv(m_locView,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(m_locProjection, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform1i(m_locEntityID, entityID);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void Renderer::beginScene(const glm::mat4& view, const glm::mat4& projection)
{
    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(m_locView,       1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(m_locProjection, 1, GL_FALSE, glm::value_ptr(projection));
}

void Renderer::setModelMatrix(const glm::mat4& model)
{
    const glm::mat3 normalMatrix = glm::inverseTranspose(glm::mat3(model));
    glUniformMatrix4fv(m_locModel, 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(m_locNormalMatrix, 1, GL_FALSE, glm::value_ptr(normalMatrix));
}

void Renderer::setHasTexture(bool has)
{
    glUniform1f(m_locHasTexture, has ? 1.0f : 0.0f);
}

void Renderer::setObjectColor(const glm::vec3& color)
{
    glUniform3f(m_locObjectColor, color.x, color.y, color.z);
}

void Renderer::setEntityID(int id)
{
    glUniform1i(m_locEntityID, id);
}

void Renderer::shutdown()
{
    glDeleteVertexArrays(1, &m_VAO);
    glDeleteBuffers(1, &m_VBO);
    glDeleteProgram(m_shaderProgram);

    m_VAO = 0;
    m_VBO = 0;
    m_shaderProgram = 0;
}

} // namespace engine
