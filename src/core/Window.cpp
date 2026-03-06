#include "Window.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <cstdio>

namespace engine {

// ── GLFW error callback (file-local) ────────────────────────────────────────

static void glfwErrorCallback(int error, const char* description)
{
    std::fprintf(stderr, "[GLFW Error %d] %s\n", error, description);
}

// ── Public API ──────────────────────────────────────────────────────────────

bool Window::init(int width, int height, const char* title)
{
    glfwSetErrorCallback(glfwErrorCallback);

    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW.\n");
        return false;
    }

    // Attempt 1: OpenGL 3.3 Core Profile (ideal)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    // Attempt 2: Drop Core Profile requirement
    if (!m_window)
    {
        std::fprintf(stderr, "Core Profile failed, retrying with Any Profile...\n");
        glfwDefaultWindowHints();
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    }

    // Attempt 3: Accept whatever the driver can give us
    if (!m_window)
    {
        std::fprintf(stderr, "GL 3.3 failed, retrying with default hints...\n");
        glfwDefaultWindowHints();
        m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    }

    if (!m_window)
    {
        std::fprintf(stderr, "Failed to create GLFW window after all attempts.\n");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // vsync

    // Load OpenGL function pointers via GLAD
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        std::fprintf(stderr, "Failed to initialize GLAD.\n");
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
        return false;
    }

    std::printf("OpenGL %s  |  GLSL %s\n",
                glGetString(GL_VERSION),
                glGetString(GL_SHADING_LANGUAGE_VERSION));

    return true;
}

void Window::update()
{
    glfwSwapBuffers(m_window);
    glfwPollEvents();
}

bool Window::shouldClose() const
{
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::getFramebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(m_window, &width, &height);
}

void Window::shutdown()
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

} // namespace engine
