#pragma once

struct GLFWwindow;

namespace engine {

class Window
{
public:
    Window()  = default;
    ~Window() = default;

    // Non-copyable, non-movable (owns a GLFW handle)
    Window(const Window&)            = delete;
    Window& operator=(const Window&) = delete;

    /// Initialise GLFW, create the window and load OpenGL via GLAD.
    /// Returns false on failure.
    bool init(int width, int height, const char* title);

    /// Poll events and swap buffers — call once per frame at the end.
    void update();

    /// Returns true when the user has requested the window to close.
    bool shouldClose() const;

    /// Destroy the window and terminate GLFW.
    void shutdown();

    /// Raw GLFW handle (needed by ImGui and other subsystems).
    GLFWwindow* getHandle() const { return m_window; }

    /// Current framebuffer dimensions.
    void getFramebufferSize(int& width, int& height) const;

private:
    GLFWwindow* m_window = nullptr;
};

} // namespace engine
