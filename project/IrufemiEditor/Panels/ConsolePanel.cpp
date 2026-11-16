#include "Panels/ConsolePanel.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "Core/Utility/Log.h"

using namespace Irufemi;

void ConsolePanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void ConsolePanel::Draw() {
    if (!editorManager_) {
        return;
    }

    ImGui::Begin(GetName(), &isOpen_);

    if (ImGui::Button("Clear")) {
        Log::ClearLogHistory();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScroll_);

    ImGui::Separator();

    ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    Log::WithLogHistory([this](const Log::LogContainer& logHistory) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(logHistory.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                const auto& logEntry = logHistory[i];
                if (logEntry.isError) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.2f, 0.2f, 1.0f));
                }
                ImGui::TextUnformatted(logEntry.message.c_str());
                if (logEntry.isError) {
                    ImGui::PopStyleColor();
                }
            }
        }

        // 自動スクロールロジック:
        // Auto-scroll が有効で、かつログの件数が増えた場合、または既に一番下にいる場合に一番下を維持する
        if (autoScroll_ && (logHistory.size() > previousLogSize_ || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
            ImGui::SetScrollHereY(1.0f);
        }

        previousLogSize_ = logHistory.size();
    });

    ImGui::EndChild();

    ImGui::End();
}
#endif // EditorMode
