#include "ConsolePanel.h"

#include "Log.h"

#include <imgui.h>

namespace engine {

void ConsolePanel::drawContents()
{
    if (ImGui::Button("Clear"))
        Log::ClearInMemory();

    ImGui::SameLine();
    m_filter.Draw("Filter", 200.0f);

    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);

    ImGui::Separator();

    ImGui::BeginChild("ConsoleScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    const auto lines = Log::GetInMemoryLines();
    for (const auto& line : lines)
    {
        if (m_filter.PassFilter(line.c_str()))
            ImGui::TextUnformatted(line.c_str());
    }

    if (m_autoScroll)
    {
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void ConsolePanel::drawWindow()
{
    ImGui::Begin("Console");
    drawContents();
    ImGui::End();
}

} // namespace engine
