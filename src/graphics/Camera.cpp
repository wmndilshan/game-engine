#include "Camera.h"

#include <cmath>

namespace engine {

// ── Construction ────────────────────────────────────────────────────────────

Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : Position(position)
    , Front(glm::vec3(0.0f, 0.0f, -1.0f))
    , Up(up)
    , Right(glm::vec3(1.0f, 0.0f, 0.0f))
    , WorldUp(up)
    , Yaw(yaw)
    , Pitch(pitch)
    , MovementSpeed(5.0f)
    , MouseSensitivity(0.1f)
{
    updateCameraVectors();
}

// ── Queries ─────────────────────────────────────────────────────────────────

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(Position, Position + Front, Up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio) const
{
    return glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
}

// ── Input processing ────────────────────────────────────────────────────────

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = MovementSpeed * deltaTime;

    switch (direction)
    {
    case CameraMovement::FORWARD:  Position += Front * velocity; break;
    case CameraMovement::BACKWARD: Position -= Front * velocity; break;
    case CameraMovement::LEFT:     Position -= Right * velocity; break;
    case CameraMovement::RIGHT:    Position += Right * velocity; break;
    }
}

void Camera::processMouseMovement(float xoffset, float yoffset,
                                  bool constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw   += xoffset;
    Pitch += yoffset;

    if (constrainPitch)
    {
        if (Pitch >  89.0f) Pitch =  89.0f;
        if (Pitch < -89.0f) Pitch = -89.0f;
    }

    updateCameraVectors();
}

// ── Internals ───────────────────────────────────────────────────────────────

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = std::cos(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));
    front.y = std::sin(glm::radians(Pitch));
    front.z = std::sin(glm::radians(Yaw)) * std::cos(glm::radians(Pitch));

    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}

} // namespace engine
