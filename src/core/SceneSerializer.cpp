#include "SceneSerializer.h"

#include "../ecs/Components.h"

#include <json.hpp>

#include <fstream>
#include <cstdio>

using json = nlohmann::json;

namespace engine {

void SceneSerializer::Serialize(Registry& registry, const std::string& filepath)
{
    json j;
    j["entities"] = json::array();

    auto& transforms = registry.getView<TransformComponent>();
    for (auto& [entity, tc] : transforms)
    {
        json entityNode;
        entityNode["id"] = entity;
        entityNode["transform"]["position"] = { tc.position.x, tc.position.y, tc.position.z };
        entityNode["transform"]["rotation"] = { tc.rotation.x, tc.rotation.y, tc.rotation.z };
        entityNode["transform"]["scale"]    = { tc.scale.x,    tc.scale.y,    tc.scale.z    };

        j["entities"].push_back(entityNode);
    }

    std::ofstream file(filepath);
    if (!file.is_open())
    {
        std::fprintf(stderr, "[SceneSerializer] Failed to open '%s' for writing.\n", filepath.c_str());
        return;
    }

    file << j.dump(4);
    file.close();

    std::printf("[SceneSerializer] Scene saved to '%s' (%zu entities)\n",
                filepath.c_str(), transforms.size());
}

void SceneSerializer::Deserialize(Registry& registry, const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        std::fprintf(stderr, "[SceneSerializer] Failed to open '%s' for reading.\n", filepath.c_str());
        return;
    }

    json j;
    file >> j;
    file.close();

    // Wipe the current scene so we start fresh
    registry.clear();

    if (!j.contains("entities") || !j["entities"].is_array())
    {
        std::fprintf(stderr, "[SceneSerializer] No 'entities' array found in '%s'.\n", filepath.c_str());
        return;
    }

    for (auto& entityNode : j["entities"])
    {
        Entity e = registry.createEntity();

        TransformComponent tc;

        if (entityNode.contains("transform"))
        {
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

        registry.addComponent(e, tc);
    }

    std::printf("[SceneSerializer] Scene loaded from '%s' (%zu entities)\n",
                filepath.c_str(), j["entities"].size());
}

} // namespace engine
