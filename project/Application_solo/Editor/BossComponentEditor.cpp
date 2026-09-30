#ifdef EditorMode
#include "Editor/BossComponentEditor.h"
#include "Combat/Boss/BossComponent.h"
#include "Core/Utility/JsonUtility.h"
#include <imgui/imgui.h>
#include <string>

void BossComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto comp = dynamic_cast<BossComponent*>(component);
    if (!comp) {
        return;
    }

    ImGui::Text("Boss Settings");
    ImGui::Separator();

    std::string path = comp->GetStatusDataPath();
    char buffer[256];
    strncpy_s(buffer, sizeof(buffer), path.c_str(), _TRUNCATE);
    buffer[sizeof(buffer) - 1] = '\0';

    if (ImGui::InputText("Status Data Path", buffer, sizeof(buffer))) {
        comp->SetStatusDataPath(buffer);
    }

    if (path != cachedPath_) {
        cachedPath_ = path;
        isJsonLoaded_ = false;
    }

    if (ImGui::Button("Reload JSON", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        isJsonLoaded_ = false;
        comp->LoadStatusFromJson();
    }
    ImGui::Spacing();

    if (!cachedPath_.empty()) {
        if (!isJsonLoaded_) {
            isJsonLoaded_ = Irufemi::JsonUtility::LoadFromFile(cachedPath_, cachedJson_);
        }

        if (isJsonLoaded_) {
            bool needSave = false;
            if (ImGui::Button("Save JSON", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                needSave = true;
            }

            if (ImGui::TreeNodeEx("Gameplay Data (Saved in JSON)", ImGuiTreeNodeFlags_DefaultOpen)) {

                float maxHp = cachedJson_.value("maxHp", 1000.0f);
                if (ImGui::DragFloat("Max HP", &maxHp, 10.0f, 1.0f, 100000.0f)) {
                    cachedJson_["maxHp"] = maxHp;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                int maxShieldCount = cachedJson_.value("maxShieldCount", 100);
                if (ImGui::DragInt("Max Shield Count", &maxShieldCount, 1, 0, 500)) {
                    cachedJson_["maxShieldCount"] = maxShieldCount;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float shieldRadius = cachedJson_.value("shieldRadius", 8.0f);
                if (ImGui::DragFloat("Shield Radius", &shieldRadius, 0.1f, 1.0f, 50.0f)) {
                    cachedJson_["shieldRadius"] = shieldRadius;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float beamInterval = cachedJson_.value("beamInterval", 10.0f);
                if (ImGui::DragFloat("Beam Interval", &beamInterval, 0.1f, 0.1f, 60.0f)) {
                    cachedJson_["beamInterval"] = beamInterval;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float beamRange = cachedJson_.value("beamRange", 1000.0f);
                if (ImGui::DragFloat("Beam Range", &beamRange, 10.0f, 10.0f, 10000.0f)) {
                    cachedJson_["beamRange"] = beamRange;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                ImGui::TreePop();
            }

            if (needSave) {
                if (Irufemi::JsonUtility::SaveToFile(cachedPath_, cachedJson_, 4)) {
                    comp->LoadStatusFromJson();
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "JSON File Not Found or Invalid!");
        }
    }
}
#endif
