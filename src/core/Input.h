#pragma once

struct GLFWwindow;

namespace engine {

/// Thin static wrapper around GLFW input polling.
class Input
{
public:
    Input()  = delete; // purely static

    /// Returns true while the key is held down.
    static bool isKeyPressed(GLFWwindow* window, int key);
};

} // namespace engine
