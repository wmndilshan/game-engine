#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "../ImGuizmo/ImGuizmo.h"

#include "../ecs/Components.h"

#include "SceneSerializer.h"

#include "../physics/PhysicsSystem.h"
#include "../scripting/PlayerController.h"

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

    // ── Physics ─────────────────────────────────────────────────────────
    m_physics.init();

    // Create a "Player" entity and attach a native script
    Entity playerEntity = m_registry.createEntity();
    {
        TransformComponent tc;
        tc.position = {0.0f, 2.0f, 0.0f};
        tc.rotation = {0.0f, 0.0f, 0.0f};
        tc.scale    = {1.0f, 1.0f, 1.0f};
        m_registry.addComponent(playerEntity, tc);

        NativeScriptComponent nsc;
        nsc.Bind<PlayerController>();
        m_registry.addComponent(playerEntity, nsc);
    }

    // Create a floor entity (static, wide platform at Y = -2)
    {
        Entity floor = m_registry.createEntity();
        TransformComponent tc;
        tc.position = {0.0f, -2.0f, 0.0f};
        tc.scale    = {10.0f, 1.0f, 10.0f};
        m_registry.addComponent(floor, tc);
        m_physics.addRigidBody(floor, m_registry, true);   // static
    }

    // Add dynamic rigid bodies to the cubes that spawnEntities() created
    {
        auto& transforms = m_registry.getView<TransformComponent>();
        auto& rigidBodies = m_registry.getView<RigidBodyComponent>();
        for (auto& [entity, tc] : transforms)
        {
            // Skip entities that already have a RigidBodyComponent (e.g. floor)
            if (rigidBodies.find(entity) != rigidBodies.end()) continue;
            m_physics.addRigidBody(entity, m_registry, false);  // dynamic
        }
    }

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
        ImGuizmo::BeginFrame();

        // ── Run native scripts (create on first use) ───────────────────
        {
            auto& scripts = m_registry.getView<NativeScriptComponent>();
            for (auto& [entity, script] : scripts)
            {
                if (!script.Instance)
                {
                    script.Instance = script.InstantiateScript ? script.InstantiateScript() : nullptr;
                    if (script.Instance)
                    {
                        script.Instance->m_Entity  = entity;
                        script.Instance->m_Registry = &m_registry;
                        script.Instance->OnCreate();
                    }
                }

                if (script.Instance)
                {
                    script.Instance->OnUpdate(m_deltaTime);
                }
            }
        }

        // ── Step physics ────────────────────────────────────────────────
        m_physics.stepPhysics(m_registry, m_deltaTime);

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
            model = glm::rotate(model, tc.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
            model = glm::rotate(model, tc.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
            model = glm::rotate(model, tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
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

            // ── ImGuizmo setup ───────────────────────────────────────────
            ImVec2 vpMin    = ImGui::GetWindowContentRegionMin();
            ImVec2 vpOffset = ImGui::GetWindowPos();
            float  vpX      = vpOffset.x + vpMin.x;
            float  vpY      = vpOffset.y + vpMin.y;
            float  vpWf     = static_cast<float>(m_framebuffer.getWidth());
            float  vpHf     = static_cast<float>(m_framebuffer.getHeight());

            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(vpX, vpY, vpWf, vpHf);

            // Keyboard shortcuts for gizmo mode (only when viewport is hovered)
            if (ImGui::IsWindowHovered())
            {
                if (ImGui::IsKeyPressed(ImGuiKey_T))
                    m_gizmoType = ImGuizmo::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_R))
                    m_gizmoType = ImGuizmo::ROTATE;
                if (ImGui::IsKeyPressed(ImGuiKey_E))
                    m_gizmoType = ImGuizmo::SCALE;
            }

            // Draw gizmo for selected entity
            if (m_hasSelection && m_gizmoType != -1)
            {
                auto& tc = m_registry.getComponent<TransformComponent>(m_selectedEntity);

                // Build the current model matrix from the entity's transform
                glm::mat4 gizmoModel(1.0f);
                gizmoModel = glm::translate(gizmoModel, tc.position);
                gizmoModel = glm::rotate(gizmoModel, tc.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                gizmoModel = glm::rotate(gizmoModel, tc.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                gizmoModel = glm::rotate(gizmoModel, tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                gizmoModel = glm::scale(gizmoModel, tc.scale);

                glm::mat4 gizmoView = m_camera.getViewMatrix();
                glm::mat4 gizmoProj = m_camera.getProjectionMatrix(fboAspect);

                ImGuizmo::Manipulate(
                    glm::value_ptr(gizmoView),
                    glm::value_ptr(gizmoProj),
                    static_cast<ImGuizmo::OPERATION>(m_gizmoType),
                    ImGuizmo::LOCAL,
                    glm::value_ptr(gizmoModel)
                );

                // If the user is dragging the gizmo, decompose the matrix back
                if (ImGuizmo::IsUsing())
                {
                    glm::vec3 translation, scale, skew;
                    glm::vec4 perspective;
                    glm::quat rotationQuat;

                    glm::decompose(gizmoModel, scale, rotationQuat, translation, skew, perspective);

                    tc.position = translation;
                    tc.rotation = glm::eulerAngles(rotationQuat);
                    tc.scale    = scale;
                }
            }

            // ── Mouse picking via entity-ID attachment ───────────────────
            // Don't pick when the gizmo is being interacted with
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && ImGui::IsWindowHovered()
                && !ImGuizmo::IsOver())
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

        // ── Main Menu Bar ───────────────────────────────────────────
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene"))
                {
                    SceneSerializer::Serialize(m_registry, "assets/scene.json");
                }
                if (ImGui::MenuItem("Load Scene"))
                {
                    m_hasSelection = false;
                    SceneSerializer::Deserialize(m_registry, "assets/scene.json");
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

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

    // Destroy native script instances
    {
        auto& scripts = m_registry.getView<NativeScriptComponent>();
        for (auto& [entity, script] : scripts)
        {
            (void)entity;
            if (script.Instance)
                script.Instance->OnDestroy();
            if (script.DestroyScript)
                script.DestroyScript(&script);
        }
    }

    m_physics.shutdown();
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
