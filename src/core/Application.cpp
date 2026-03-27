#include "Application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "../ImGuizmo/ImGuizmo.h"

#include "../ecs/Components.h"

#include "SceneSerializer.h"

#include "../physics/PhysicsSystem.h"
#include "../scripting/PlayerController.h"
#include "../scripting/ScriptRegistry.h"
#include "../audio/AudioSystem.h"

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <json.hpp>

namespace engine {

static constexpr const char* kTempPlayStatePath = "assets/temp_play_state.json";

static bool DirectoryExists(const char* path)
{
    struct stat sb;
    if (stat(path, &sb) != 0)
        return false;
    return (sb.st_mode & S_IFDIR) != 0;
}

static std::string GetCurrentWorkingDirectory()
{
    char buffer[1024] = {};

#if defined(_WIN32)
    if (_getcwd(buffer, sizeof(buffer)) == nullptr)
        return {};
#else
    if (getcwd(buffer, sizeof(buffer)) == nullptr)
        return {};
#endif

    return std::string(buffer);
}

static void FixWorkingDirectoryForAssetsIfNeeded()
{
    if (DirectoryExists("assets"))
        return;

    if (!DirectoryExists("../assets") && !DirectoryExists("..\\assets"))
        return;

#if defined(_WIN32)
    _chdir("..");
#else
    chdir("..");
#endif
}

static reactphysics3d::Quaternion ToRp3dQuaternionFromEulerXYZ(const glm::vec3& eulerRadians)
{
    glm::mat4 rot(1.0f);
    rot = glm::rotate(rot, eulerRadians.x, glm::vec3(1.0f, 0.0f, 0.0f));
    rot = glm::rotate(rot, eulerRadians.y, glm::vec3(0.0f, 1.0f, 0.0f));
    rot = glm::rotate(rot, eulerRadians.z, glm::vec3(0.0f, 0.0f, 1.0f));

    const glm::quat q = glm::quat_cast(rot);
    return reactphysics3d::Quaternion(q.x, q.y, q.z, q.w);
}

static void DestroyAllScripts(Registry& registry)
{
    auto& scripts = registry.getView<NativeScriptComponent>();
    for (auto& [entity, script] : scripts)
    {
        (void)entity;
        if (script.Instance)
            script.Instance->OnDestroy();
        if (script.DestroyScript)
            script.DestroyScript(&script);
    }
}

static void ResetPhysicsBodiesFromEcs(Registry& registry)
{
    auto& transforms = registry.getView<TransformComponent>();
    auto& bodies     = registry.getView<RigidBodyComponent>();

    for (auto& [entity, rb] : bodies)
    {
        if (!rb.body) continue;

        auto it = transforms.find(entity);
        if (it == transforms.end()) continue;

        const TransformComponent& tc = it->second;

        const reactphysics3d::Vector3 pos(tc.position.x, tc.position.y, tc.position.z);
        const reactphysics3d::Quaternion rot = ToRp3dQuaternionFromEulerXYZ(tc.rotation);

        rb.body->setTransform(reactphysics3d::Transform(pos, rot));
        rb.body->setLinearVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
        rb.body->setAngularVelocity(reactphysics3d::Vector3(0.0f, 0.0f, 0.0f));
    }
}

static void RestoreTransformsFromTempFile(Registry& registry)
{
    using json = nlohmann::json;

    std::ifstream file(kTempPlayStatePath);
    if (!file.is_open())
    {
        GE_ERROR(std::string("[SceneState] Failed to open '") + kTempPlayStatePath + "' for reading.");
        return;
    }

    json j;
    file >> j;

    if (!j.contains("entities") || !j["entities"].is_array())
        return;

    auto& transforms = registry.getView<TransformComponent>();

    for (auto& entityNode : j["entities"])
    {
        if (!entityNode.contains("id")) continue;
        const Entity e = entityNode["id"].get<Entity>();

        auto it = transforms.find(e);
        if (it == transforms.end()) continue;

        TransformComponent& tc = it->second;

        if (!entityNode.contains("transform")) continue;
        auto& t = entityNode["transform"];

        if (t.contains("position") && t["position"].is_array() && t["position"].size() == 3)
        {
            tc.position.x = t["position"][0].get<float>();
            tc.position.y = t["position"][1].get<float>();
            tc.position.z = t["position"][2].get<float>();
        }

        if (t.contains("rotation") && t["rotation"].is_array() && t["rotation"].size() == 3)
        {
            tc.rotation.x = t["rotation"][0].get<float>();
            tc.rotation.y = t["rotation"][1].get<float>();
            tc.rotation.z = t["rotation"][2].get<float>();
        }

        if (t.contains("scale") && t["scale"].is_array() && t["scale"].size() == 3)
        {
            tc.scale.x = t["scale"][0].get<float>();
            tc.scale.y = t["scale"][1].get<float>();
            tc.scale.z = t["scale"][2].get<float>();
        }
    }
}

// ── ImGui helpers ───────────────────────────────────────────────────────────

static glm::vec3 GetSelectionFocusPoint(Registry& registry, bool hasSelection, Entity selectedEntity)
{
    if (!hasSelection)
        return glm::vec3(0.0f, 0.0f, 0.0f);

    auto& transforms = registry.getView<TransformComponent>();
    auto it = transforms.find(selectedEntity);
    if (it == transforms.end())
        return glm::vec3(0.0f, 0.0f, 0.0f);

    return it->second.position;
}

static void SnapCameraToDirection(Camera& camera, const glm::vec3& focusPoint, const glm::vec3& direction)
{
    const float distance = glm::max(4.0f, glm::length(camera.Position - focusPoint));
    camera.Position = focusPoint - glm::normalize(direction) * distance;
    camera.lookAt(focusPoint);
}

static bool ScreenPointToWorldRay(
    const Camera& camera,
    const ImVec2& imageMin,
    const ImVec2& imageSize,
    const ImVec2& mousePos,
    glm::vec3& outOrigin,
    glm::vec3& outDirection)
{
    if (imageSize.x <= 0.0f || imageSize.y <= 0.0f)
        return false;

    const float aspect = imageSize.x / imageSize.y;
    const float x = ((mousePos.x - imageMin.x) / imageSize.x) * 2.0f - 1.0f;
    const float y = 1.0f - ((mousePos.y - imageMin.y) / imageSize.y) * 2.0f;

    glm::vec4 clipCoords(x, y, -1.0f, 1.0f);
    glm::mat4 invProjection = glm::inverse(camera.getProjectionMatrix(aspect));
    glm::vec4 viewCoords = invProjection * clipCoords;
    viewCoords = glm::vec4(viewCoords.x, viewCoords.y, -1.0f, 0.0f);

    glm::mat4 invView = glm::inverse(camera.getViewMatrix());
    glm::vec4 worldCoords = invView * viewCoords;

    outOrigin = camera.Position;
    outDirection = glm::normalize(glm::vec3(worldCoords));
    return true;
}

static bool RayPlaneIntersection(
    const glm::vec3& rayOrigin,
    const glm::vec3& rayDirection,
    const glm::vec3& planePoint,
    const glm::vec3& planeNormal,
    glm::vec3& outPoint)
{
    const float denom = glm::dot(rayDirection, planeNormal);
    if (std::abs(denom) < 0.0001f)
        return false;

    const float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
    if (t < 0.0f)
        return false;

    outPoint = rayOrigin + rayDirection * t;
    return true;
}

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
        m_registry.addComponent(e, TagComponent{"Left Cube"});
        TransformComponent tc;
        tc.position = {-2.5f, 0.0f, 0.0f};
        tc.rotation = {0.0f, 0.0f, 0.0f};
        m_registry.addComponent(e, tc);

        ColorComponent cc;
        cc.color = {0.85f, 0.25f, 0.25f};
        m_registry.addComponent(e, cc);
        MaterialComponent mc;
        mc.name = "Red Cube";
        mc.albedo = cc.color;
        m_registry.addComponent(e, mc);
    }

    // Centre cube
    {
        Entity e = m_registry.createEntity();
        m_registry.addComponent(e, TagComponent{"Center Cube"});
        TransformComponent tc;
        tc.position = {0.0f, 0.0f, 0.0f};
        tc.rotation = {0.5f, 0.3f, 0.0f};
        m_registry.addComponent(e, tc);

        ColorComponent cc;
        cc.color = {0.25f, 0.85f, 0.35f};
        m_registry.addComponent(e, cc);
        MaterialComponent mc;
        mc.name = "Green Cube";
        mc.albedo = cc.color;
        m_registry.addComponent(e, mc);
    }

    // Right cube
    {
        Entity e = m_registry.createEntity();
        m_registry.addComponent(e, TagComponent{"Right Cube"});
        TransformComponent tc;
        tc.position = {2.5f, 0.0f, 0.0f};
        tc.rotation = {0.0f, 0.7f, 0.2f};
        m_registry.addComponent(e, tc);

        ColorComponent cc;
        cc.color = {0.25f, 0.45f, 0.95f};
        m_registry.addComponent(e, cc);
        MaterialComponent mc;
        mc.name = "Blue Cube";
        mc.albedo = cc.color;
        m_registry.addComponent(e, mc);
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

    FixWorkingDirectoryForAssetsIfNeeded();
    Log::Init();
    {
        const std::string cwd = GetCurrentWorkingDirectory();
        if (!cwd.empty())
            GE_INFO(std::string("Working directory: ") + cwd);
    }

    if (!m_window.init(800, 600, "Engine3D"))
    {
        Log::Shutdown();
        return EXIT_FAILURE;
    }

    if (!m_renderer.init())
    {
        m_window.shutdown();
        Log::Shutdown();
        return EXIT_FAILURE;
    }

    // Store 'this' so the static mouse callback can reach us
    glfwSetWindowUserPointer(m_window.getHandle(), this);

    // Normal cursor so ImGui windows are usable;
    // camera look is driven by right-mouse-drag (handled in mouseCallback)
    glfwSetCursorPosCallback(m_window.getHandle(), mouseCallback);

    initImGui();
    spawnEntities();

    // ── Audio ─────────────────────────────────────────────────────────
    m_audio.init();

    // ── Physics ─────────────────────────────────────────────────────────
    m_physics.init();

    // Create a "Player" entity and attach a native script
    Entity playerEntity = m_registry.createEntity();
    {
        m_registry.addComponent(playerEntity, TagComponent{"Player"});
        TransformComponent tc;
        tc.position = {0.0f, 2.0f, 0.0f};
        tc.rotation = {0.0f, 0.0f, 0.0f};
        tc.scale    = {1.0f, 1.0f, 1.0f};
        m_registry.addComponent(playerEntity, tc);

        NativeScriptComponent nsc;
        ConfigureNativeScriptComponent(nsc, NativeScriptType::PlayerController);
        m_registry.addComponent(playerEntity, nsc);

        // Test sound: play once when entering Play mode
        AudioSourceComponent asc;
        asc.filepath = "assets/sound.wav";
        asc.playOnAwake = true;
        m_registry.addComponent(playerEntity, asc);
    }

    // Create a floor entity (static, wide platform at Y = -2)
    {
        Entity floor = m_registry.createEntity();
        m_registry.addComponent(floor, TagComponent{"Floor"});
        TransformComponent tc;
        tc.position = {0.0f, -2.0f, 0.0f};
        tc.scale    = {10.0f, 1.0f, 10.0f};
        m_registry.addComponent(floor, tc);

        ColorComponent cc;
        cc.color = {0.35f, 0.35f, 0.35f};
        m_registry.addComponent(floor, cc);
        MaterialComponent mc;
        mc.name = "Floor";
        mc.albedo = cc.color;
        m_registry.addComponent(floor, mc);

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
        GE_ERROR("Failed to create framebuffer!");
        m_window.shutdown();
        Log::Shutdown();
        return EXIT_FAILURE;
    }

    // Load textures
    if (!m_crateTexture.init("assets/crate.jpg"))
    {
        GE_WARN("Crate texture not found; cubes will be untextured.");
    }

    // Load 3D model via Assimp
    if (!m_model.loadModel("assets/cube.obj"))
    {
        GE_WARN("Model not loaded; only ECS cubes will render.");
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
            GE_WARN("Skybox not loaded; sky will be plain colour.");
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

        auto& transformView = m_registry.getView<TransformComponent>();
        if (m_hasSelection && transformView.find(m_selectedEntity) == transformView.end())
        {
            m_hasSelection = false;
            m_isDraggingEntity = false;
        }

        // ── Start ImGui frame ───────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        // ── Main Editor Host Window (engine-like layout without docking) ──
        const ImGuiViewport* mainVp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(mainVp->WorkPos);
        ImGui::SetNextWindowSize(mainVp->WorkSize);

        ImGuiWindowFlags hostFlags = 0;
        hostFlags |= ImGuiWindowFlags_NoTitleBar;
        hostFlags |= ImGuiWindowFlags_NoCollapse;
        hostFlags |= ImGuiWindowFlags_NoResize;
        hostFlags |= ImGuiWindowFlags_NoMove;
        hostFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
        hostFlags |= ImGuiWindowFlags_NoNavFocus;
        hostFlags |= ImGuiWindowFlags_MenuBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("Editor", nullptr, hostFlags);
        ImGui::PopStyleVar(2);

        // Menu bar
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene"))
                    SceneSerializer::Serialize(m_registry, "assets/scene.json");

                if (ImGui::MenuItem("Load Scene"))
                {
                    if (m_SceneState == SceneState::Play)
                    {
                        DestroyAllScripts(m_registry);
                        m_SceneState = SceneState::Edit;
                    }
                    m_hasSelection = false;
                    m_isDraggingEntity = false;
                    SceneSerializer::Deserialize(m_registry, "assets/scene.json");
                    m_physics.rebuildWorld(m_registry);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // ── Toolbar (Edit vs Play) ─────────────────────────────────────
        ImGui::BeginChild("##Toolbar", ImVec2(0.0f, 44.0f), true);
        {
            if (m_SceneState == SceneState::Edit)
            {
                if (ImGui::Button("Play"))
                {
                    SceneSerializer::Serialize(m_registry, kTempPlayStatePath);

                    // Reset audio "play on awake" so it triggers each Play
                    {
                        auto& audioSources = m_registry.getView<AudioSourceComponent>();
                        for (auto& [entity, audio] : audioSources)
                        {
                            (void)entity;
                            audio.hasPlayed = false;
                        }
                    }

                    // Ensure physics bodies match current edited transforms
                    ResetPhysicsBodiesFromEcs(m_registry);

                    // Reset scripts so they start fresh in Play
                    DestroyAllScripts(m_registry);

                    m_SceneState = SceneState::Play;
                }
            }
            else
            {
                if (ImGui::Button("Stop"))
                {
                    m_SceneState = SceneState::Edit;

                    // Stop runtime scripts (next Play will re-create them)
                    DestroyAllScripts(m_registry);
                    SceneSerializer::Deserialize(m_registry, kTempPlayStatePath);
                    m_physics.rebuildWorld(m_registry);
                    m_hasSelection = false;
                    m_isDraggingEntity = false;
                }
            }
        }
        ImGui::EndChild();

        // ── Main region (Hierarchy | Viewport | Inspector) ─────────────
        static float bottomHeight = 240.0f;
        const float splitterThickness = 6.0f;
        const float minMainHeight = 150.0f;
        const float minBottomHeight = 120.0f;

        ImVec2 editorAvail = ImGui::GetContentRegionAvail();
        const float maxBottomHeight = (editorAvail.y > (minMainHeight + splitterThickness))
            ? (editorAvail.y - minMainHeight - splitterThickness)
            : minBottomHeight;
        if (bottomHeight > maxBottomHeight) bottomHeight = maxBottomHeight;
        if (bottomHeight < minBottomHeight) bottomHeight = minBottomHeight;

        const float mainHeight = editorAvail.y - bottomHeight - splitterThickness;
        ImGui::BeginChild("##Main", ImVec2(0.0f, mainHeight), false);
        {
            ImGuiTableFlags tableFlags = 0;
            tableFlags |= ImGuiTableFlags_Resizable;
            tableFlags |= ImGuiTableFlags_BordersInnerV;
            tableFlags |= ImGuiTableFlags_SizingStretchProp;

            if (ImGui::BeginTable("##MainSplit", 3, tableFlags))
            {
                ImGui::TableSetupColumn("Hierarchy", ImGuiTableColumnFlags_WidthFixed, 260.0f);
                ImGui::TableSetupColumn("Viewport", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthFixed, 320.0f);
                ImGui::TableNextRow();

                // Column 0: Hierarchy
                ImGui::TableSetColumnIndex(0);
                ImGui::BeginChild("Scene Hierarchy", ImVec2(0.0f, 0.0f), true);
                {
                    auto& view = m_registry.getView<TransformComponent>();
                    auto& tags = m_registry.getView<TagComponent>();

                    if (ImGui::Button("Add Empty"))
                    {
                        const Entity entity = m_registry.createEntity();
                        m_registry.addComponent(entity, TagComponent{"Empty Entity"});
                        m_registry.addComponent(entity, TransformComponent{});
                        m_selectedEntity = entity;
                        m_hasSelection = true;
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Add Cube"))
                    {
                        const Entity entity = m_registry.createEntity();
                        m_registry.addComponent(entity, TagComponent{"Cube"});
                        m_registry.addComponent(entity, TransformComponent{});
                        m_registry.addComponent(entity, ColorComponent{});
                        m_registry.addComponent(entity, MaterialComponent{});
                        m_selectedEntity = entity;
                        m_hasSelection = true;
                    }

                    ImGui::Separator();

                    for (auto& [entity, tc] : view)
                    {
                        (void)tc;
                        auto tagIt = tags.find(entity);
                        const std::string label = (tagIt != tags.end() ? tagIt->second.tag : ("Entity " + std::to_string(entity)))
                            + "##" + std::to_string(entity);
                        if (ImGui::Selectable(label.c_str(), m_hasSelection && m_selectedEntity == entity))
                        {
                            m_selectedEntity = entity;
                            m_hasSelection   = true;
                        }
                    }
                }
                ImGui::EndChild();

                // Column 1: Viewport
                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                ImGui::BeginChild("Viewport", ImVec2(0.0f, 0.0f), true);
                ImGui::PopStyleVar();
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
                    if (vpW > 0 && vpH > 0)
                    {
                        ImTextureID texId = (ImTextureID)(intptr_t)m_framebuffer.getTexture();
                        ImGui::Image(
                            texId,
                            ImVec2(avail.x, avail.y),
                            ImVec2(0.0f, 1.0f),
                            ImVec2(1.0f, 0.0f)
                        );
                    }

                    const ImVec2 imageMin = ImGui::GetItemRectMin();
                    const ImVec2 imageMax = ImGui::GetItemRectMax();
                    const ImVec2 imageSize = ImGui::GetItemRectSize();
                    const bool viewportHovered = ImGui::IsItemHovered();
                    const ImVec2 mousePos = ImGui::GetMousePos();
                    const float mouseWheel = ImGui::GetIO().MouseWheel;

                // ── ImGuizmo setup ─────────────────────────────────────
                float  vpX      = imageMin.x;
                float  vpY      = imageMin.y;
                float  vpWf     = imageSize.x;
                float  vpHf     = imageSize.y;

                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist();
                ImGuizmo::SetRect(vpX, vpY, vpWf, vpHf);

                // Keyboard shortcuts for gizmo mode (only when viewport is hovered)
                if (viewportHovered)
                {
                    if (ImGui::IsKeyPressed(ImGuiKey_T))
                        m_gizmoType = ImGuizmo::TRANSLATE;
                    if (ImGui::IsKeyPressed(ImGuiKey_R))
                        m_gizmoType = ImGuizmo::ROTATE;
                    if (ImGui::IsKeyPressed(ImGuiKey_E))
                        m_gizmoType = ImGuizmo::SCALE;
                }

                // Draw gizmo for selected entity (Edit mode only)
                if (m_SceneState == SceneState::Edit && m_hasSelection && m_gizmoType != -1)
                {
                    auto& tc = m_registry.getComponent<TransformComponent>(m_selectedEntity);

                    const float embeddedFboAspect = (m_framebuffer.getHeight() > 0)
                        ? static_cast<float>(m_framebuffer.getWidth()) / static_cast<float>(m_framebuffer.getHeight())
                        : 1.0f;

                    glm::mat4 gizmoModel(1.0f);
                    gizmoModel = glm::translate(gizmoModel, tc.position);
                    gizmoModel = glm::rotate(gizmoModel, tc.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                    gizmoModel = glm::rotate(gizmoModel, tc.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                    gizmoModel = glm::rotate(gizmoModel, tc.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                    gizmoModel = glm::scale(gizmoModel, tc.scale);

                    glm::mat4 gizmoView = m_camera.getViewMatrix();
                    glm::mat4 gizmoProj = m_camera.getProjectionMatrix(embeddedFboAspect);

                    ImGuizmo::Manipulate(
                        glm::value_ptr(gizmoView),
                        glm::value_ptr(gizmoProj),
                        static_cast<ImGuizmo::OPERATION>(m_gizmoType),
                        ImGuizmo::LOCAL,
                        glm::value_ptr(gizmoModel)
                    );

                    if (ImGuizmo::IsUsing())
                    {
                        glm::vec3 translation, scale, skew;
                        glm::vec4 perspective;
                        glm::quat rotationQuat;

                        glm::decompose(gizmoModel, scale, rotationQuat, translation, skew, perspective);

                        tc.position = translation;
                        const glm::mat4 rotMat = glm::mat4_cast(rotationQuat);
                        float rx = 0.0f, ry = 0.0f, rz = 0.0f;
                        glm::extractEulerAngleXYZ(rotMat, rx, ry, rz);
                        tc.rotation = glm::vec3(rx, ry, rz);
                        tc.scale    = scale;
                        ResetPhysicsBodiesFromEcs(m_registry);
                    }
                }

                // ── Mouse picking via entity-ID attachment ─────────────
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                    && viewportHovered
                    && !ImGuizmo::IsOver())
                {
                    float localX = mousePos.x - imageMin.x;
                    float localY = mousePos.y - imageMin.y;

                    int pixelX = static_cast<int>(localX);
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

                            if (m_SceneState == SceneState::Edit && !ImGuizmo::IsUsing())
                            {
                                auto& transforms = m_registry.getView<TransformComponent>();
                                auto selectedIt = transforms.find(m_selectedEntity);
                                if (selectedIt != transforms.end())
                                {
                                    glm::vec3 rayOrigin, rayDirection, hitPoint;
                                    if (ScreenPointToWorldRay(m_camera, imageMin, imageSize, mousePos, rayOrigin, rayDirection))
                                    {
                                        m_dragPlanePoint = selectedIt->second.position;
                                        m_dragPlaneNormal = m_camera.Front;
                                        if (std::abs(glm::dot(rayDirection, m_dragPlaneNormal)) < 0.1f)
                                            m_dragPlaneNormal = glm::vec3(0.0f, 1.0f, 0.0f);

                                        if (RayPlaneIntersection(rayOrigin, rayDirection, m_dragPlanePoint, m_dragPlaneNormal, hitPoint))
                                        {
                                            m_dragOffset = selectedIt->second.position - hitPoint;
                                            m_isDraggingEntity = true;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            m_hasSelection = false;
                            m_isDraggingEntity = false;
                        }
                    }
                }

                if (m_isDraggingEntity && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    m_isDraggingEntity = false;

                if (viewportHovered && std::abs(mouseWheel) > 0.001f)
                    m_camera.Position += m_camera.Front * (mouseWheel * 1.5f);

                if (m_SceneState == SceneState::Edit
                    && m_isDraggingEntity
                    && m_hasSelection
                    && viewportHovered
                    && !ImGuizmo::IsUsing())
                {
                    auto& transforms = m_registry.getView<TransformComponent>();
                    auto selectedIt = transforms.find(m_selectedEntity);
                    if (selectedIt != transforms.end())
                    {
                        glm::vec3 rayOrigin, rayDirection, hitPoint;
                        if (ScreenPointToWorldRay(m_camera, imageMin, imageSize, mousePos, rayOrigin, rayDirection)
                            && RayPlaneIntersection(rayOrigin, rayDirection, m_dragPlanePoint, m_dragPlaneNormal, hitPoint))
                        {
                            selectedIt->second.position = hitPoint + m_dragOffset;
                            ResetPhysicsBodiesFromEcs(m_registry);
                        }
                    }
                }

                ImGui::SetCursorScreenPos(ImVec2(imageMax.x - 124.0f, imageMin.y + 10.0f));
                ImGui::BeginChild("##ViewIndicator", ImVec2(114.0f, 146.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                {
                    ImDrawList* drawList = ImGui::GetWindowDrawList();
                    const ImVec2 overlayPos = ImGui::GetWindowPos();
                    const ImVec2 center(overlayPos.x + 57.0f, overlayPos.y + 36.0f);
                    const glm::mat3 viewRotation(m_camera.getViewMatrix());

                    auto drawAxis = [&](const glm::vec3& axis, ImU32 color, const char* label)
                    {
                        const glm::vec3 cameraAxis = viewRotation * axis;
                        const ImVec2 end(center.x + cameraAxis.x * 18.0f, center.y - cameraAxis.y * 18.0f);
                        drawList->AddLine(center, end, color, 2.0f);
                        drawList->AddText(ImVec2(end.x + 2.0f, end.y - 7.0f), color, label);
                    };

                    drawAxis(glm::vec3(1.0f, 0.0f, 0.0f), IM_COL32(220, 80, 80, 255), "X");
                    drawAxis(glm::vec3(0.0f, 1.0f, 0.0f), IM_COL32(80, 220, 120, 255), "Y");
                    drawAxis(glm::vec3(0.0f, 0.0f, 1.0f), IM_COL32(90, 140, 255, 255), "Z");

                    const glm::vec3 focusPoint = GetSelectionFocusPoint(m_registry, m_hasSelection, m_selectedEntity);

                    ImGui::SetCursorPos(ImVec2(36.0f, 46.0f));
                    if (ImGui::Button("Top", ImVec2(40.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, focusPoint, glm::vec3(0.0f, -1.0f, 0.0f));

                    if (ImGui::Button("Left", ImVec2(34.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, focusPoint, glm::vec3(1.0f, 0.0f, 0.0f));
                    ImGui::SameLine();
                    if (ImGui::Button("Persp", ImVec2(40.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, glm::vec3(0.0f, 0.0f, 0.0f), glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)));
                    ImGui::SameLine();
                    if (ImGui::Button("Right", ImVec2(34.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, focusPoint, glm::vec3(-1.0f, 0.0f, 0.0f));

                    if (ImGui::Button("Front", ImVec2(48.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, focusPoint, glm::vec3(0.0f, 0.0f, 1.0f));
                    ImGui::SameLine();
                    if (ImGui::Button("Back", ImVec2(48.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, focusPoint, glm::vec3(0.0f, 0.0f, -1.0f));
                    if (ImGui::Button("Bottom", ImVec2(48.0f, 0.0f)))
                        SnapCameraToDirection(m_camera, focusPoint, glm::vec3(0.0f, 1.0f, 0.0f));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%s", m_hasSelection ? "Focus Sel" : "Focus Origin");
                }
                ImGui::EndChild();

                }
                ImGui::EndChild();

                // Column 2: Inspector
                ImGui::TableSetColumnIndex(2);
                ImGui::BeginChild("Inspector", ImVec2(0.0f, 0.0f), true);
                {
                    if (m_hasSelection)
                    {
                        auto& tc = m_registry.getComponent<TransformComponent>(m_selectedEntity);
                        auto& tags = m_registry.getView<TagComponent>();
                        auto& colors = m_registry.getView<ColorComponent>();
                        auto& materials = m_registry.getView<MaterialComponent>();
                        auto& rigidBodies = m_registry.getView<RigidBodyComponent>();
                        auto& audioSources = m_registry.getView<AudioSourceComponent>();
                        auto& scripts = m_registry.getView<NativeScriptComponent>();

                        auto& tag = tags[m_selectedEntity];
                        char tagBuffer[128] = {};
                        std::snprintf(tagBuffer, sizeof(tagBuffer), "%s", tag.tag.c_str());

                        ImGui::Text("Entity %u", m_selectedEntity);
                        if (ImGui::InputText("Name", tagBuffer, sizeof(tagBuffer)))
                            tag.tag = tagBuffer;
                        ImGui::Separator();

                        ImGui::DragFloat3("Position", glm::value_ptr(tc.position), 0.1f);
                        ImGui::DragFloat3("Rotation", glm::value_ptr(tc.rotation), 0.01f);
                        ImGui::DragFloat3("Scale",    glm::value_ptr(tc.scale),    0.1f);

                        auto colorIt = colors.find(m_selectedEntity);
                        if (colorIt != colors.end())
                        {
                            ImGui::Separator();
                            if (ImGui::ColorEdit3("Color", glm::value_ptr(colorIt->second.color)))
                            {
                                auto materialIt = materials.find(m_selectedEntity);
                                if (materialIt != materials.end())
                                    materialIt->second.albedo = colorIt->second.color;
                            }
                        }
                        else if (ImGui::Button("Add Color"))
                        {
                            m_registry.addComponent(m_selectedEntity, ColorComponent{});
                        }

                        ImGui::Separator();
                        auto materialIt = materials.find(m_selectedEntity);
                        if (materialIt != materials.end())
                        {
                            char materialNameBuffer[128] = {};
                            char materialTextureBuffer[260] = {};
                            std::snprintf(materialNameBuffer, sizeof(materialNameBuffer), "%s", materialIt->second.name.c_str());
                            std::snprintf(materialTextureBuffer, sizeof(materialTextureBuffer), "%s", materialIt->second.texturePath.c_str());

                            ImGui::Text("Material");
                            ImGui::ColorButton("##MaterialPreview", ImVec4(materialIt->second.albedo.r, materialIt->second.albedo.g, materialIt->second.albedo.b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(48.0f, 48.0f));
                            ImGui::SameLine();
                            ImGui::BeginGroup();
                            if (ImGui::InputText("Material Name", materialNameBuffer, sizeof(materialNameBuffer)))
                                materialIt->second.name = materialNameBuffer;
                            if (ImGui::ColorEdit3("Albedo", glm::value_ptr(materialIt->second.albedo)))
                            {
                                auto legacyColorIt = colors.find(m_selectedEntity);
                                if (legacyColorIt != colors.end())
                                    legacyColorIt->second.color = materialIt->second.albedo;
                            }
                            ImGui::Checkbox("Use Texture", &materialIt->second.useTexture);
                            if (ImGui::InputText("Texture Path", materialTextureBuffer, sizeof(materialTextureBuffer)))
                                materialIt->second.texturePath = materialTextureBuffer;
                            ImGui::EndGroup();
                        }
                        else if (ImGui::Button("Add Material"))
                        {
                            MaterialComponent material;
                            auto legacyColorIt = colors.find(m_selectedEntity);
                            if (legacyColorIt != colors.end())
                                material.albedo = legacyColorIt->second.color;
                            m_registry.addComponent(m_selectedEntity, material);
                        }

                        ImGui::Separator();
                        auto rbIt = rigidBodies.find(m_selectedEntity);
                        if (rbIt != rigidBodies.end())
                        {
                            bool isStatic = rbIt->second.isStatic;
                            if (ImGui::Checkbox("Static Body", &isStatic))
                            {
                                rbIt->second.isStatic = isStatic;
                                m_physics.rebuildWorld(m_registry);
                            }
                        }
                        else if (ImGui::Button("Add Rigid Body"))
                        {
                            m_physics.addRigidBody(m_selectedEntity, m_registry, false);
                        }

                        ImGui::Separator();
                        auto audioIt = audioSources.find(m_selectedEntity);
                        if (audioIt != audioSources.end())
                        {
                            char audioPathBuffer[260] = {};
                            std::snprintf(audioPathBuffer, sizeof(audioPathBuffer), "%s", audioIt->second.filepath.c_str());
                            if (ImGui::InputText("Audio Path", audioPathBuffer, sizeof(audioPathBuffer)))
                                audioIt->second.filepath = audioPathBuffer;
                            ImGui::Checkbox("Play On Awake", &audioIt->second.playOnAwake);
                        }
                        else if (ImGui::Button("Add Audio Source"))
                        {
                            AudioSourceComponent audio;
                            audio.filepath = "assets/sound.wav";
                            m_registry.addComponent(m_selectedEntity, audio);
                        }

                        ImGui::Separator();
                        auto scriptIt = scripts.find(m_selectedEntity);
                        if (scriptIt != scripts.end())
                        {
                            int scriptIndex = scriptIt->second.Type == NativeScriptType::PlayerController ? 1 : 0;
                            const char* scriptOptions[] = {"None", "PlayerController"};
                            if (ImGui::Combo("Script", &scriptIndex, scriptOptions, IM_ARRAYSIZE(scriptOptions)))
                            {
                                if (scriptIt->second.Instance)
                                    scriptIt->second.Instance->OnDestroy();

                                if (scriptIndex == 0)
                                {
                                    if (scriptIt->second.DestroyScript)
                                        scriptIt->second.DestroyScript(&scriptIt->second);
                                    scripts.erase(m_selectedEntity);
                                }
                                else
                                {
                                    ConfigureNativeScriptComponent(scriptIt->second, NativeScriptType::PlayerController);
                                }
                            }
                        }
                        else if (ImGui::Button("Add Player Script"))
                        {
                            NativeScriptComponent script;
                            ConfigureNativeScriptComponent(script, NativeScriptType::PlayerController);
                            m_registry.addComponent(m_selectedEntity, script);
                        }
                    }
                    else
                    {
                        ImGui::Text("Select an entity to view properties.");
                    }
                }
                ImGui::EndChild();

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();

        // Bottom splitter (drag to resize Console/Debug area)
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::InvisibleButton("##BottomSplitter", ImVec2(-1.0f, splitterThickness));
        ImGui::PopStyleVar(2);
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        if (ImGui::IsItemActive())
        {
            bottomHeight = bottomHeight - ImGui::GetIO().MouseDelta.y;
            if (bottomHeight < minBottomHeight) bottomHeight = minBottomHeight;
            if (bottomHeight > maxBottomHeight) bottomHeight = maxBottomHeight;
        }

        // ── Bottom tabs (Console | Debug) ─────────────────────────────
        ImGui::BeginChild("##Bottom", ImVec2(0.0f, bottomHeight), true);
        {
            if (ImGui::BeginTabBar("BottomTabs"))
            {
                if (ImGui::BeginTabItem("Console"))
                {
                    m_console.drawContents();
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Engine Debug"))
                {
                    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                    ImGui::Text("ECS Entities: %zu", m_registry.getView<TransformComponent>().size());
                    ImGui::Text("Model Meshes: %zu", m_model.meshes.size());
                    ImGui::Text("Viewport: %dx%d", m_framebuffer.getWidth(), m_framebuffer.getHeight());
                    ImGui::Separator();
                    ImGui::Text("Camera Pos: (%.1f, %.1f, %.1f)",
                                 m_camera.Position.x, m_camera.Position.y, m_camera.Position.z);
                    ImGui::Text("Yaw: %.1f  Pitch: %.1f", m_camera.Yaw, m_camera.Pitch);
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();

        ImGui::End(); // Editor host

        // ── Run scripts + physics only during Play ─────────────────────
        if (m_SceneState == SceneState::Play)
        {
            // Audio update loop (play-on-awake)
            {
                auto& audioSources = m_registry.getView<AudioSourceComponent>();
                for (auto& [entity, audio] : audioSources)
                {
                    (void)entity;
                    if (audio.playOnAwake && !audio.hasPlayed)
                    {
                        if (m_audio.playSound(audio.filepath))
                            audio.hasPlayed = true;
                    }
                }
            }

            // Run native scripts (create on first use)
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
                            script.Instance->m_Audio = &m_audio;
                            script.Instance->OnCreate();
                        }
                    }

                    if (script.Instance)
                        script.Instance->OnUpdate(m_deltaTime);
                }
            }

            // Step physics
            m_physics.stepPhysics(m_registry, m_deltaTime);
        }

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
        m_renderer.setObjectColor({0.8f, 0.5f, 0.3f});
        m_renderer.setEntityID(-1);
        {
            glm::mat4 modelMat(1.0f);
            modelMat = glm::translate(modelMat, glm::vec3(0.0f, 0.0f, 0.0f));
            modelMat = glm::rotate(modelMat, time * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            m_renderer.setModelMatrix(modelMat);
            m_model.draw();
        }

        // Draw ECS cubes/entities (each writes its entity ID for picking)
        const bool hasCrateTexture = m_crateTexture.getID() != 0;
        auto& transforms = m_registry.getView<TransformComponent>();
        auto& colors = m_registry.getView<ColorComponent>();
        auto& materials = m_registry.getView<MaterialComponent>();
        for (auto& [entity, tc] : transforms)
        {
            bool useTexture = false;
            glm::vec3 objectColor{0.8f, 0.5f, 0.3f};

            auto materialIt = materials.find(entity);
            if (materialIt != materials.end())
            {
                objectColor = materialIt->second.albedo;
                useTexture = materialIt->second.useTexture && hasCrateTexture;
            }
            else
            {
                auto cit = colors.find(entity);
                if (cit != colors.end())
                    objectColor = cit->second.color;
            }

            m_renderer.setObjectColor(objectColor);
            m_renderer.setHasTexture(useTexture);
            if (useTexture)
                m_crateTexture.bind(0);

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
    m_audio.shutdown();
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

    Log::Shutdown();

    return EXIT_SUCCESS;
}

} // namespace engine
