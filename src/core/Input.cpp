#include "Input.h"

#include <GLFW/glfw3.h>

namespace engine {

bool Input::isKeyPressed(GLFWwindow* window, int key)
{
    return glfwGetKey(window, key) == GLFW_PRESS;
}

} // namespace engine
