#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "../ecs/Components.h"

#include <cstdio>
#include <cstdlib>

namespace engine {

// ── ImGui helpers ───────────────────────────────────────────────────────────

void Application::initImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window.getHandle(), true);
    ImGui_ImplOpenGL3_Init("#version 140");
}

void Application::shutdownImGui()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// ── ECS setup ───────────────────────────────────────────────────────────────

void Application::spawnEntities()
{
    // Left cube
    {
        Entity e = m_registry.createEntity();
        TransformComponent tc;
        tc.position = {-2.5f, 0.0f, 0.0f};
        tc.rotation = {0.0f, 0.0f, 0.0f};
        m_registry.addComponent(e, tc);
    }

    // Centre cube
    {
        Entity e = m_registry.createEntity();
        TransformComponent tc;
        tc.position = {0.0f, 0.0f, 0.0f};
        tc.rotation = {0.5f, 0.3f, 0.0f};
        m_registry.addComponent(e, tc);
    }

    // Right cube
    {
        Entity e = m_registry.createEntity();
        TransformComponent tc;
        tc.position = {2.5f, 0.0f, 0.0f};
        tc.rotation = {0.0f, 0.7f, 0.2f};
        m_registry.addComponent(e, tc);
    }
}

// ── Input ───────────────────────────────────────────────────────────────────

void Application::processInput()
{
    GLFWwindow* win = m_window.getHandle();

    if (Input::isKeyPressed(win, GLFW_KEY_W))
        m_camera.processKeyboard(CameraMovement::FORWARD, m_deltaTime);
    if (Input::isKeyPressed(win, GLFW_KEY_S))
        m_camera.processKeyboard(CameraMovement::BACKWARD, m_deltaTime);
    if (Input::isKeyPressed(win, GLFW_KEY_A))
        m_camera.processKeyboard(CameraMovement::LEFT, m_deltaTime);
    if (Input::isKeyPressed(win, GLFW_KEY_D))
        m_camera.processKeyboard(CameraMovement::RIGHT, m_deltaTime);

    // ESC to release cursor & close
    if (Input::isKeyPressed(win, GLFW_KEY_ESCAPE))
        glfwSetWindowShouldClose(win, GLFW_TRUE);
}

// ── GLFW mouse callback (static) ───────────────────────────────────────────

void Application::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    float xposf = static_cast<float>(xpos);
    float yposf = static_cast<float>(ypos);

    if (app->m_firstMouse)
    {
        app->m_lastX = xposf;
        app->m_lastY = yposf;
        app->m_firstMouse = false;
    }

    float xoffset = xposf - app->m_lastX;
    float yoffset = app->m_lastY - yposf; // reversed: y goes bottom-to-top
    app->m_lastX = xposf;
    app->m_lastY = yposf;

    app->m_camera.processMouseMovement(xoffset, yoffset);
}

// ── Main entry point ────────────────────────────────────────────────────────

int Application::run()
{
    // ── Initialise subsystems ───────────────────────────────────────────────

    if (!m_window.init(800, 600, "Engine3D"))
        return EXIT_FAILURE;

    if (!m_renderer.init())
    {
        m_window.shutdown();
        return EXIT_FAILURE;
    }

    // Store 'this' so the static mouse callback can reach us
    glfwSetWindowUserPointer(m_window.getHandle(), this);

    // Capture & hide the cursor for FPS-style mouse look
    glfwSetInputMode(m_window.getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(m_window.getHandle(), mouseCallback);

    initImGui();
    spawnEntities();

    // Load textures
    if (!m_crateTexture.init("assets/crate.jpg"))
    {
        std::fprintf(stderr, "Warning: crate texture not found, cubes will be untextured.\n");
    }

    // Load 3D model via Assimp
    if (!m_model.loadModel("assets/cube.obj"))
    {
        std::fprintf(stderr, "Warning: model not loaded, only ECS cubes will render.\n");
    }

    // ── Game loop ───────────────────────────────────────────────────────────

    while (!m_window.shouldClose())
    {
        // Timing
        float currentFrame = static_cast<float>(glfwGetTime());
        m_deltaTime = currentFrame - m_lastFrame;
        m_lastFrame = currentFrame;

        glfwPollEvents();
        processInput();

        // Clear
        m_renderer.clear({0.15f, 0.15f, 0.15f, 1.0f});

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Camera matrices
        int fbWidth = 0, fbHeight = 0;
        m_window.getFramebufferSize(fbWidth, fbHeight);
        float aspect = (fbHeight > 0)
                           ? static_cast<float>(fbWidth) / static_cast<float>(fbHeight)
                           : 1.0f;

        glm::mat4 view       = m_camera.getViewMatrix();
        glm::mat4 projection = m_camera.getProjectionMatrix(aspect);

        float time = static_cast<float>(glfwGetTime());

        // ── Draw loaded model ───────────────────────────────────────────
        m_renderer.beginScene(view, projection);
        m_renderer.setHasTexture(false); // model uses flat colour (no texture yet)
        {
            glm::mat4 modelMat(1.0f);
            // Place the loaded model slightly in front of the camera start pos
            modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 0.0f));
            modelMat = glm::rotate(modelMat, time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            m_renderer.setModelMatrix(modelMat);
            m_model.draw();
        }

        // ── Draw ECS textured cubes ─────────────────────────────────────
        // Bind the crate texture to unit 0
        m_crateTexture.bind(0);

        // Iterate every entity that has a TransformComponent
        auto& transforms = m_registry.getView<TransformComponent>();
        for (auto& [entity, tc] : transforms)
        {
            // Build model matrix: translate → rotate (animated) → scale
            glm::mat4 model(1.0f);
            model = glm::translate(model, tc.position);
            model = glm::rotate(model, time + tc.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, time + tc.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, time + tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, tc.scale);

            m_renderer.drawCube(model, view, projection);
        }

        // ImGui debug overlay
        ImGui::Begin("Engine Debug");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("ECS Entities: %zu", transforms.size());
        ImGui::Text("Model Meshes: %zu", m_model.meshes.size());
        ImGui::Separator();
        ImGui::Text("Camera Pos: (%.1f, %.1f, %.1f)",
                     m_camera.Position.x, m_camera.Position.y, m_camera.Position.z);
        ImGui::Text("Yaw: %.1f  Pitch: %.1f", m_camera.Yaw, m_camera.Pitch);
        ImGui::End();

        // Render ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        m_window.update();
    }

    // ── Cleanup ─────────────────────────────────────────────────────────────

    shutdownImGui();
    m_model.shutdown();
    m_crateTexture.shutdown();
    m_renderer.shutdown();
    m_window.shutdown();

    return EXIT_SUCCESS;
}

} // namespace engine
