#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "../ecs/Components.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

    // Only look around when right mouse button is held
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != GLFW_PRESS)
    {
        app->m_firstMouse = true;
        return;
    }

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

    // Normal cursor so ImGui windows are usable;
    // camera look is driven by right-mouse-drag (handled in mouseCallback)
    glfwSetCursorPosCallback(m_window.getHandle(), mouseCallback);

    initImGui();
    spawnEntities();

    // Create off-screen framebuffer for the 3D viewport
    if (!m_framebuffer.init(800, 600))
    {
        std::fprintf(stderr, "Failed to create framebuffer!\n");
        m_window.shutdown();
        return EXIT_FAILURE;
    }

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

    // Load skybox cubemap
    {
        std::vector<std::string> faces = {
            "assets/skybox/right.jpg",
            "assets/skybox/left.jpg",
            "assets/skybox/top.jpg",
            "assets/skybox/bottom.jpg",
            "assets/skybox/front.jpg",
            "assets/skybox/back.jpg"
        };
        m_skybox = new Skybox();
        if (!m_skybox->init(faces))
        {
            std::fprintf(stderr, "Warning: skybox not loaded, sky will be plain colour.\n");
            delete m_skybox;
            m_skybox = nullptr;
        }
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

        // ── Start ImGui frame ───────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── Bind FBO — render 3D scene into off-screen texture ──────────
        m_framebuffer.bind();
        glEnable(GL_DEPTH_TEST);
        m_renderer.clear({0.15f, 0.15f, 0.15f, 1.0f});
        m_framebuffer.clearEntityIDs();  // clear entity-ID attachment to -1

        // Camera matrices (use FBO aspect ratio, not window)
        float fboAspect = (m_framebuffer.getHeight() > 0)
                              ? static_cast<float>(m_framebuffer.getWidth())
                                / static_cast<float>(m_framebuffer.getHeight())
                              : 1.0f;

        glm::mat4 view       = m_camera.getViewMatrix();
        glm::mat4 projection = m_camera.getProjectionMatrix(fboAspect);

        float time = static_cast<float>(glfwGetTime());

        // Draw loaded model (flat colour, not a pickable entity)
        m_renderer.beginScene(view, projection);
        m_renderer.setHasTexture(false);
        m_renderer.setEntityID(-1);
        {
            glm::mat4 modelMat(1.0f);
            modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 0.0f));
            modelMat = glm::rotate(modelMat, time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            m_renderer.setModelMatrix(modelMat);
            m_model.draw();
        }

        // Draw ECS textured cubes (each writes its entity ID for picking)
        m_crateTexture.bind(0);
        auto& transforms = m_registry.getView<TransformComponent>();
        for (auto& [entity, tc] : transforms)
        {
            glm::mat4 model(1.0f);
            model = glm::translate(model, tc.position);
            model = glm::rotate(model, time + tc.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, time + tc.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, time + tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
            model = glm::scale(model, tc.scale);

            m_renderer.drawCube(model, view, projection, static_cast<int>(entity));
        }

        // Draw skybox (rendered last with depth trick so it appears behind everything)
        if (m_skybox)
        {
            m_skybox->draw(view, projection);
        }

        // ── Unbind FBO — back to default framebuffer ────────────────────
        m_framebuffer.unbind();

        // Clear the main window to a dark background
        int winW = 0, winH = 0;
        m_window.getFramebufferSize(winW, winH);
        glViewport(0, 0, winW, winH);
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ── Viewport window (shows the FBO texture) ─────────────────────
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");
        {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            int vpW = static_cast<int>(avail.x);
            int vpH = static_cast<int>(avail.y);

            // Rescale FBO if viewport changed size
            if (vpW > 0 && vpH > 0)
            {
                m_framebuffer.rescale(vpW, vpH);
            }

            // Draw the FBO colour texture — UVs flipped vertically for OpenGL
            ImTextureID texId = static_cast<ImTextureID>(m_framebuffer.getTexture());
            ImGui::Image(
                texId,
                ImVec2(static_cast<float>(m_framebuffer.getWidth()),
                       static_cast<float>(m_framebuffer.getHeight())),
                ImVec2(0.0f, 1.0f),   // UV top-left  (flipped)
                ImVec2(1.0f, 0.0f)    // UV bot-right (flipped)
            );

            // ── Mouse picking via entity-ID attachment ───────────────────
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
            {
                ImVec2 viewportMin    = ImGui::GetWindowContentRegionMin();
                ImVec2 viewportOffset = ImGui::GetWindowPos();
                ImVec2 mousePos       = ImGui::GetMousePos();

                // Mouse position relative to the viewport content region
                float localX = mousePos.x - (viewportOffset.x + viewportMin.x);
                float localY = mousePos.y - (viewportOffset.y + viewportMin.y);

                int pixelX = static_cast<int>(localX);
                // Flip Y: OpenGL origin is bottom-left, ImGui is top-left
                int pixelY = m_framebuffer.getHeight() - static_cast<int>(localY);

                if (pixelX >= 0 && pixelX < m_framebuffer.getWidth() &&
                    pixelY >= 0 && pixelY < m_framebuffer.getHeight())
                {
                    int pickedID = m_framebuffer.readPixel(
                        static_cast<std::uint32_t>(pixelX),
                        static_cast<std::uint32_t>(pixelY)
                    );

                    if (pickedID >= 0)
                    {
                        m_selectedEntity = static_cast<std::uint32_t>(pickedID);
                        m_hasSelection   = true;
                    }
                    else
                    {
                        m_hasSelection = false;
                    }
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // ── Scene Hierarchy ─────────────────────────────────────────────
        ImGui::Begin("Scene Hierarchy");
        {
            auto& view = m_registry.getView<TransformComponent>();
            for (auto& [entity, tc] : view)
            {
                std::string label = "Entity " + std::to_string(entity);
                if (ImGui::Selectable(label.c_str(), m_hasSelection && m_selectedEntity == entity))
                {
                    m_selectedEntity = entity;
                    m_hasSelection   = true;
                }
            }
        }
        ImGui::End();

        // ── Inspector ───────────────────────────────────────────────────
        ImGui::Begin("Inspector");
        {
            if (m_hasSelection)
            {
                auto& tc = m_registry.getComponent<TransformComponent>(m_selectedEntity);

                ImGui::Text("Entity %u", m_selectedEntity);
                ImGui::Separator();

                ImGui::DragFloat3("Position", glm::value_ptr(tc.position), 0.1f);
                ImGui::DragFloat3("Rotation", glm::value_ptr(tc.rotation), 0.01f);
                ImGui::DragFloat3("Scale",    glm::value_ptr(tc.scale),    0.1f);
            }
            else
            {
                ImGui::Text("Select an entity to view properties.");
            }
        }
        ImGui::End();

        // ── Debug overlay ───────────────────────────────────────────────
        ImGui::Begin("Engine Debug");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("ECS Entities: %zu", transforms.size());
        ImGui::Text("Model Meshes: %zu", m_model.meshes.size());
        ImGui::Text("Viewport: %dx%d", m_framebuffer.getWidth(), m_framebuffer.getHeight());
        ImGui::Separator();
        ImGui::Text("Camera Pos: (%.1f, %.1f, %.1f)",
                     m_camera.Position.x, m_camera.Position.y, m_camera.Position.z);
        ImGui::Text("Yaw: %.1f  Pitch: %.1f", m_camera.Yaw, m_camera.Pitch);
        ImGui::End();

        // ── Render ImGui & present ──────────────────────────────────────
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        m_window.update();
    }

    // ── Cleanup ─────────────────────────────────────────────────────────────

    shutdownImGui();
    if (m_skybox)
    {
        m_skybox->shutdown();
        delete m_skybox;
        m_skybox = nullptr;
    }
    m_framebuffer.shutdown();
    m_model.shutdown();
    m_crateTexture.shutdown();
    m_renderer.shutdown();
    m_window.shutdown();

    return EXIT_SUCCESS;
}

} // namespace engine
