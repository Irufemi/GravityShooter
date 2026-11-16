#include "Panels/EngineSettingsPanel.h"

#ifdef EditorMode
#include "Core/EditorManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/UI/DebugUI.h"
#include "imgui/imgui.h"

using namespace Irufemi;

void EngineSettingsPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void EngineSettingsPanel::Draw() {
    if (!editorManager_) {
        return;
    }

    ImGui::Begin(GetName(), &isOpen_, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    auto* engine = editorManager_->GetEngine();
    if (engine) {
        auto* ui = engine->GetDebugUI();
        if (ui) {
            ui->DrawCommonEngineTabs(engine);
        }
    }

    ImGui::End();
}
#endif // EditorMode
