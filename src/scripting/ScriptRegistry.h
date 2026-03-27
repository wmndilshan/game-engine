#pragma once

#include "../ecs/Components.h"
#include "PlayerController.h"

#include <string_view>

namespace engine {

inline const char* NativeScriptTypeToString(NativeScriptType type)
{
    switch (type)
    {
        case NativeScriptType::PlayerController:
            return "PlayerController";
        case NativeScriptType::None:
        default:
            return "None";
    }
}

inline NativeScriptType NativeScriptTypeFromString(std::string_view value)
{
    if (value == "PlayerController")
        return NativeScriptType::PlayerController;
    return NativeScriptType::None;
}

inline void ConfigureNativeScriptComponent(NativeScriptComponent& component, NativeScriptType type)
{
    component = NativeScriptComponent{};
    component.Type = type;

    switch (type)
    {
        case NativeScriptType::PlayerController:
            component.Bind<PlayerController>();
            component.Type = NativeScriptType::PlayerController;
            break;
        case NativeScriptType::None:
        default:
            break;
    }
}

} // namespace engine
