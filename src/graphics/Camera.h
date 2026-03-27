#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine {

enum class CameraMovement
{
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT
};

class Camera
{
public:
    // ── Public state (readable by ImGui overlays, etc.) ─────────────────
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw;
    float Pitch;
    float MovementSpeed;
    float MouseSensitivity;

    // ── Construction ────────────────────────────────────────────────────

    explicit Camera(glm::vec3 position  = glm::vec3(0.0f, 0.0f, 3.0f),
                    glm::vec3 up        = glm::vec3(0.0f, 1.0f, 0.0f),
                    float     yaw       = -90.0f,
                    float     pitch     =   0.0f);

    // ── Queries ─────────────────────────────────────────────────────────

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio) const;

    // ── Input processing ────────────────────────────────────────────────

    void processKeyboard(CameraMovement direction, float deltaTime);
    void processMouseMovement(float xoffset, float yoffset,
                              bool constrainPitch = true);
    void lookAt(const glm::vec3& target);

private:
    void updateCameraVectors();
};

} // namespace engine
