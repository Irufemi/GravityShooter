#ifdef EditorMode
#include "Editor/GravityPlayerComponentEditor.h"
#include "Player/GravityPlayerComponent.h"
#include "Core/Utility/JsonUtility.h"
#include <imgui/imgui.h>
#include <string>

void GravityPlayerComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto comp = dynamic_cast<GravityPlayerComponent*>(component);
    if (!comp) {
        return;
    }

    ImGui::Text("Gravity Player Settings");
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

    // JSONファイルから直接ロードして編集・保存する
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

                int maxHp = cachedJson_.value("maxHp", 100);
                if (ImGui::DragInt("Max HP", &maxHp, 1, 1, 10000)) {
                    cachedJson_["maxHp"] = maxHp;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                int maxOrbitCount = cachedJson_.value("maxOrbitCount", 5);
                if (ImGui::DragInt("Max Orbit Count", &maxOrbitCount, 1, 1, 50)) {
                    cachedJson_["maxOrbitCount"] = maxOrbitCount;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float pullRadius = cachedJson_.value("pullRadius", 100.0f);
                if (ImGui::DragFloat("Pull Radius", &pullRadius, 1.0f, 1.0f, 1000.0f)) {
                    cachedJson_["pullRadius"] = pullRadius;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float throwInterval = cachedJson_.value("throwInterval", 0.15f);
                if (ImGui::DragFloat("Throw Interval", &throwInterval, 0.01f, 0.01f, 5.0f)) {
                    cachedJson_["throwInterval"] = throwInterval;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float orbitRadiusMin = cachedJson_.value("orbitRadiusMin", 2.0f);
                if (ImGui::DragFloat("Orbit Radius Min", &orbitRadiusMin, 0.1f, 0.1f, 20.0f)) {
                    cachedJson_["orbitRadiusMin"] = orbitRadiusMin;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                float orbitRadiusMax = cachedJson_.value("orbitRadiusMax", 4.0f);
                if (ImGui::DragFloat("Orbit Radius Max", &orbitRadiusMax, 0.1f, 0.1f, 20.0f)) {
                    cachedJson_["orbitRadiusMax"] = orbitRadiusMax;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    needSave = true;
                }

                ImGui::TreePop();
            }

            // 編集完了時またはボタン押下時のみファイルへの保存とコンポーネントへの反映を行う
            if (needSave) {
                if (Irufemi::JsonUtility::SaveToFile(cachedPath_, cachedJson_, 4)) {
                    // コンポーネント側も即時反映
                    comp->LoadStatusFromJson();
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "JSON File Not Found or Invalid!");
        }
    }
}
#endif
