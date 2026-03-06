#pragma once

#include "../ecs/ECS.h"

#include <string>

namespace engine {

/// Saves and loads the ECS scene state to/from a JSON text file.
class SceneSerializer
{
public:
    /// Write every entity and its TransformComponent to a JSON file.
    static void Serialize(Registry& registry, const std::string& filepath);

    /// Clear the registry and rebuild it from a JSON file.
    static void Deserialize(Registry& registry, const std::string& filepath);
};

} // namespace engine
