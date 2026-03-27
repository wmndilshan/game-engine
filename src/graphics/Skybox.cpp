#include "Skybox.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include "../core/Log.h"

#include <string>

namespace engine {

// ── Skybox shader sources (GLSL 1.40 — OpenGL 3.1 compatible) ───────────────
//
// The vertex shader forces depth to the maximum (w/w = 1.0) so the skybox is
// always behind every other object.  The fragment shader samples a cubemap
// using the interpolated 3-D direction.

static const char* skyboxVertexSource = R"(
#version 140

in vec3 aPos;

out vec3 TexCoords;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    TexCoords = aPos;
    vec4 pos  = projection * view * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)";

static const char* skyboxFragmentSource = R"(
#version 140

in vec3 TexCoords;

out vec4 color0;    // colour attachment 0 - scene colour
out int  color1;    // colour attachment 1 - entity ID (R32I)

uniform samplerCube skybox;

void main()
{
    color0 = texture(skybox, TexCoords);
    color1 = -1;    // skybox is not a pickable entity
}
)";

// ── 36-vertex cube (positions only) ─────────────────────────────────────────

static const float skyboxVertices[] = {
    // Back face
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    // Front face
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    // Left face
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,

    // Right face
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,

    // Top face
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,

    // Bottom face
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
};

// ── Helper: compile & link the skybox shader program ────────────────────────

static unsigned int compileSkyboxShader()
{
    // Vertex shader
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &skyboxVertexSource, nullptr);
    glCompileShader(vs);
    {
        int ok = 0;
        glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(vs, sizeof(log), nullptr, log);
            GE_ERROR(std::string("[Skybox] Vertex shader error: ") + log);
        }
    }

    // Fragment shader
    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &skyboxFragmentSource, nullptr);
    glCompileShader(fs);
    {
        int ok = 0;
        glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetShaderInfoLog(fs, sizeof(log), nullptr, log);
            GE_ERROR(std::string("[Skybox] Fragment shader error: ") + log);
        }
    }

    // Link
    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);

    // Bind attribute location before linking (GLSL 1.40 — no layout qualifiers)
    glBindAttribLocation(program, 0, "aPos");

    // Bind fragment data locations for MRT (colour attachment 0 & 1)
    glBindFragDataLocation(program, 0, "color0");
    glBindFragDataLocation(program, 1, "color1");

    glLinkProgram(program);
    {
        int ok = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (!ok)
        {
            char log[512];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            GE_ERROR(std::string("[Skybox] Shader link error: ") + log);
        }
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    return program;
}

// ── Skybox public API ───────────────────────────────────────────────────────

bool Skybox::init(const std::vector<std::string>& faces)
{
    // ── Compile shader ──────────────────────────────────────────────────
    m_shaderProgram = compileSkyboxShader();
    if (m_shaderProgram == 0)
        return false;

    // ── Set up VAO / VBO ────────────────────────────────────────────────
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    // aPos — location 0, 3 floats, stride = 3 * sizeof(float)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

    glBindVertexArray(0);

    // ── Load cubemap faces ──────────────────────────────────────────────
    glGenTextures(1, &m_cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);

    // Do NOT flip vertically for cubemaps
    stbi_set_flip_vertically_on_load(false);

    for (unsigned int i = 0; i < faces.size(); ++i)
    {
        int width = 0, height = 0, nrChannels = 0;
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, static_cast<GLint>(format),
                width, height, 0,
                format, GL_UNSIGNED_BYTE, data
            );
            stbi_image_free(data);
        }
        else
        {
            GE_WARN(std::string("[Skybox] Failed to load face: ") + faces[i]);
            stbi_image_free(data);
            return false;
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return true;
}

void Skybox::draw(const glm::mat4& view, const glm::mat4& projection)
{
    // Render the skybox at maximum depth so it never covers scene geometry.
    glDepthFunc(GL_LEQUAL);

    glUseProgram(m_shaderProgram);

    // Strip the translation component — skybox stays centred on the camera.
    glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

    glUniformMatrix4fv(
        glGetUniformLocation(m_shaderProgram, "view"),
        1, GL_FALSE, glm::value_ptr(skyboxView)
    );
    glUniformMatrix4fv(
        glGetUniformLocation(m_shaderProgram, "projection"),
        1, GL_FALSE, glm::value_ptr(projection)
    );

    // Bind cubemap to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "skybox"), 0);

    glBindVertexArray(m_VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // Restore default depth function
    glDepthFunc(GL_LESS);
}

void Skybox::shutdown()
{
    if (m_cubemapTexture) { glDeleteTextures(1, &m_cubemapTexture); m_cubemapTexture = 0; }
    if (m_VBO)            { glDeleteBuffers(1, &m_VBO);             m_VBO = 0;            }
    if (m_VAO)            { glDeleteVertexArrays(1, &m_VAO);        m_VAO = 0;            }
    if (m_shaderProgram)  { glDeleteProgram(m_shaderProgram);       m_shaderProgram = 0;  }
}

} // namespace engine
