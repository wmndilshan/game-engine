#include "SceneSerializer.h"

#include "../ecs/Components.h"
#include "../scripting/ScriptRegistry.h"

#include <json.hpp>

#include <fstream>

#include "Log.h"

using json = nlohmann::json;

namespace engine {

void SceneSerializer::Serialize(Registry& registry, const std::string& filepath)
{
    json j;
    j["entities"] = json::array();

    auto& transforms = registry.getView<TransformComponent>();
    auto& tags = registry.getView<TagComponent>();
    auto& colors = registry.getView<ColorComponent>();
    auto& materials = registry.getView<MaterialComponent>();
    auto& rigidBodies = registry.getView<RigidBodyComponent>();
    auto& audioSources = registry.getView<AudioSourceComponent>();
    auto& scripts = registry.getView<NativeScriptComponent>();

    for (auto& [entity, tc] : transforms)
    {
        json entityNode;
        entityNode["id"] = entity;

        auto tagIt = tags.find(entity);
        entityNode["tag"] = (tagIt != tags.end()) ? tagIt->second.tag : ("Entity " + std::to_string(entity));

        entityNode["transform"]["position"] = {tc.position.x, tc.position.y, tc.position.z};
        entityNode["transform"]["rotation"] = {tc.rotation.x, tc.rotation.y, tc.rotation.z};
        entityNode["transform"]["scale"] = {tc.scale.x, tc.scale.y, tc.scale.z};

        auto colorIt = colors.find(entity);
        if (colorIt != colors.end())
            entityNode["color"] = {colorIt->second.color.x, colorIt->second.color.y, colorIt->second.color.z};

        auto materialIt = materials.find(entity);
        if (materialIt != materials.end())
        {
            entityNode["material"]["name"] = materialIt->second.name;
            entityNode["material"]["albedo"] = {materialIt->second.albedo.x, materialIt->second.albedo.y, materialIt->second.albedo.z};
            entityNode["material"]["useTexture"] = materialIt->second.useTexture;
            entityNode["material"]["texturePath"] = materialIt->second.texturePath;
        }

        auto rbIt = rigidBodies.find(entity);
        if (rbIt != rigidBodies.end())
            entityNode["rigidbody"]["isStatic"] = rbIt->second.isStatic;

        auto audioIt = audioSources.find(entity);
        if (audioIt != audioSources.end())
        {
            entityNode["audio"]["filepath"] = audioIt->second.filepath;
            entityNode["audio"]["playOnAwake"] = audioIt->second.playOnAwake;
        }

        auto scriptIt = scripts.find(entity);
        if (scriptIt != scripts.end() && scriptIt->second.Type != NativeScriptType::None)
            entityNode["script"] = NativeScriptTypeToString(scriptIt->second.Type);

        j["entities"].push_back(entityNode);
    }

    std::ofstream file(filepath);
    if (!file.is_open())
    {
        GE_ERROR(std::string("[SceneSerializer] Failed to open '") + filepath + "' for writing.");
        return;
    }

    file << j.dump(4);
    file.close();

    GE_INFO(std::string("[SceneSerializer] Scene saved to '") + filepath + "' (" + std::to_string(transforms.size()) + " entities)");
}

void SceneSerializer::Deserialize(Registry& registry, const std::string& filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open())
    {
        GE_ERROR(std::string("[SceneSerializer] Failed to open '") + filepath + "' for reading.");
        return;
    }

    json j;
    file >> j;
    file.close();

    registry.clear();

    if (!j.contains("entities") || !j["entities"].is_array())
    {
        GE_WARN(std::string("[SceneSerializer] No 'entities' array found in '") + filepath + "'.");
        return;
    }

    for (auto& entityNode : j["entities"])
    {
        const Entity e = entityNode.contains("id")
            ? registry.createEntity(entityNode["id"].get<Entity>())
            : registry.createEntity();

        TagComponent tag;
        if (entityNode.contains("tag") && entityNode["tag"].is_string())
            tag.tag = entityNode["tag"].get<std::string>();
        else
            tag.tag = "Entity " + std::to_string(e);

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

        registry.addComponent(e, tag);
        registry.addComponent(e, tc);

        if (entityNode.contains("color") && entityNode["color"].is_array() && entityNode["color"].size() == 3)
        {
            ColorComponent cc;
            cc.color.x = entityNode["color"][0].get<float>();
            cc.color.y = entityNode["color"][1].get<float>();
            cc.color.z = entityNode["color"][2].get<float>();
            registry.addComponent(e, cc);
        }

        if (entityNode.contains("material") && entityNode["material"].is_object())
        {
            MaterialComponent material;
            auto& m = entityNode["material"];
            if (m.contains("name"))
                material.name = m["name"].get<std::string>();
            if (m.contains("albedo") && m["albedo"].is_array() && m["albedo"].size() == 3)
            {
                material.albedo.x = m["albedo"][0].get<float>();
                material.albedo.y = m["albedo"][1].get<float>();
                material.albedo.z = m["albedo"][2].get<float>();
            }
            if (m.contains("useTexture"))
                material.useTexture = m["useTexture"].get<bool>();
            if (m.contains("texturePath"))
                material.texturePath = m["texturePath"].get<std::string>();
            registry.addComponent(e, material);
        }

        if (entityNode.contains("rigidbody") && entityNode["rigidbody"].is_object())
        {
            RigidBodyComponent rb;
            if (entityNode["rigidbody"].contains("isStatic"))
                rb.isStatic = entityNode["rigidbody"]["isStatic"].get<bool>();
            registry.addComponent(e, rb);
        }

        if (entityNode.contains("audio") && entityNode["audio"].is_object())
        {
            AudioSourceComponent audio;
            if (entityNode["audio"].contains("filepath"))
                audio.filepath = entityNode["audio"]["filepath"].get<std::string>();
            if (entityNode["audio"].contains("playOnAwake"))
                audio.playOnAwake = entityNode["audio"]["playOnAwake"].get<bool>();
            audio.hasPlayed = false;
            registry.addComponent(e, audio);
        }

        if (entityNode.contains("script") && entityNode["script"].is_string())
        {
            NativeScriptComponent script;
            ConfigureNativeScriptComponent(script, NativeScriptTypeFromString(entityNode["script"].get<std::string>()));
            if (script.Type != NativeScriptType::None)
                registry.addComponent(e, script);
        }
    }

    GE_INFO(std::string("[SceneSerializer] Scene loaded from '") + filepath + "' (" + std::to_string(j["entities"].size()) + " entities)");
}

} // namespace engine
