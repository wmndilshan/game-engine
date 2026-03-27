#pragma once

#include "Window.h"
#include "Input.h"
#include "Log.h"
#include "ConsolePanel.h"
#include "../graphics/Renderer.h"
#include "../graphics/Camera.h"
#include "../graphics/Texture.h"
#include "../graphics/Model.h"
#include "../graphics/Framebuffer.h"
#include "../graphics/Skybox.h"
#include "../ecs/ECS.h"
#include "../physics/PhysicsSystem.h"
#include "../audio/AudioSystem.h"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <cstdint>

#include "imgui.h"
#include "../ImGuizmo/ImGuizmo.h"

struct GLFWwindow;

namespace engine {

class Application
{
public:
    Application()  = default;
    ~Application() = default;

    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;

    /// Initialise all subsystems and enter the main loop.
    /// Returns the process exit code.
    int run();

    enum class SceneState
    {
        Edit,
        Play
    };

private:
    void initImGui();
    void shutdownImGui();
    void spawnEntities();
    void processInput();

    /// GLFW mouse-move callback (static — uses GLFW user pointer to reach *this).
    static void mouseCallback(GLFWwindow* window, double xpos, double ypos);

    Window        m_window;
    Renderer      m_renderer;
    Registry      m_registry;
    PhysicsSystem m_physics;
    AudioSystem   m_audio;
    Camera        m_camera{glm::vec3(0.0f, 0.0f, 7.0f)};
    Texture     m_crateTexture;
    Model       m_model;
    Framebuffer m_framebuffer;
    Skybox*     m_skybox = nullptr;

    // Timing
    float m_deltaTime = 0.0f;
    float m_lastFrame = 0.0f;

    // Mouse state
    float m_lastX     = 400.0f;
    float m_lastY     = 300.0f;
    bool  m_firstMouse = true;

    // Entity selection (for Inspector panel)
    std::uint32_t m_selectedEntity = 0;
    bool          m_hasSelection   = false;

    // Gizmo operation (-1 = none, else ImGuizmo::OPERATION)
    int m_gizmoType = -1;

    // Direct viewport drag state
    bool      m_isDraggingEntity = false;
    glm::vec3 m_dragOffset{0.0f, 0.0f, 0.0f};
    glm::vec3 m_dragPlanePoint{0.0f, 0.0f, 0.0f};
    glm::vec3 m_dragPlaneNormal{0.0f, 0.0f, 1.0f};

    // Editor / runtime scene state
    SceneState m_SceneState = SceneState::Edit;

    ConsolePanel m_console;
};

} // namespace engine
