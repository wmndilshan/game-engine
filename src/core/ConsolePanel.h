#pragma once

#include <imgui.h>

namespace engine {

class ConsolePanel
{
public:
    void drawWindow();
    void drawContents();

private:
    bool m_autoScroll = true;
    ImGuiTextFilter m_filter;
};

} // namespace engine
