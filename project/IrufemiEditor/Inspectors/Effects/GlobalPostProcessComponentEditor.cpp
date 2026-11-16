#include "GlobalPostProcessComponentEditor.h"

#ifdef EditorMode
#include "Framework/Component/Effect/GlobalPostProcessComponent.h"
#include "Commands/EditorActionManager.h"
#include "Core/ICommand.h"
#include "UI/ComponentUIHelpers.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"
#include <imgui/imgui.h>

using namespace Irufemi;

namespace {

/**
 * @class AddOverrideCommand
 * @brief GlobalPostProcessComponent へのエフェクト設定追加を記録・Undo/Redoするコマンド
 */
class AddOverrideCommand : public ICommand {
public:
    AddOverrideCommand(GlobalPostProcessComponent* comp, std::shared_ptr<IPostProcessSettings> setting, size_t index)
        : comp_(comp), setting_(setting), index_(index) {}

    void Do() override {
        comp_->InsertOverride(index_, setting_);
    }

    void Undo() override {
        comp_->RemoveOverride(index_);
    }

private:
    GlobalPostProcessComponent* comp_;
    std::shared_ptr<IPostProcessSettings> setting_;
    size_t index_;
};

/**
 * @class RemoveOverrideCommand
 * @brief GlobalPostProcessComponent からのエフェクト設定削除を記録・Undo/Redoするコマンド
 */
class RemoveOverrideCommand : public ICommand {
public:
    RemoveOverrideCommand(GlobalPostProcessComponent* comp, std::shared_ptr<IPostProcessSettings> setting, size_t index)
        : comp_(comp), setting_(setting), index_(index) {}

    void Do() override {
        comp_->RemoveOverride(index_);
    }

    void Undo() override {
        comp_->InsertOverride(index_, setting_);
    }

private:
    GlobalPostProcessComponent* comp_;
    std::shared_ptr<IPostProcessSettings> setting_;
    size_t index_;
};

} // namespace

void GlobalPostProcessComponentEditor::DrawFloatProperty(const char* label, float& value, float defaultValue,
                                                         float minVal, float maxVal,
                                                         EditorActionManager* actionManager) {
    ImGui::TableNextRow();
    ComponentUIHelpers::DrawPropertyLabel(label);

    ImGui::TableSetColumnIndex(1);
    ImGui::PushItemWidth(-1);
    std::string dragId = std::string("##") + label;
    if (ImGui::DragFloat(dragId.c_str(), &value, 0.01f, minVal, maxVal)) {
        // Undo tracking is handled below
    }
    ImGui::PopItemWidth();

    ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &value,
                                          std::function<void(const float&)>([&value](const float& v) { value = v; }));

    std::string resetId = std::string("##Reset") + label;
    ComponentUIHelpers::DrawPropertyResetButton(resetId.c_str(), value != defaultValue, [&]() {
        float oldVal = value;
        ComponentUIHelpers::PushInstantUndo(actionManager, oldVal, defaultValue,
                                            std::function<void(const float&)>([&value](const float& v) { value = v; }));
    });
}

void GlobalPostProcessComponentEditor::DrawBoolProperty(const char* label, bool& value, bool defaultValue,
                                                        EditorActionManager* actionManager) {
    ImGui::TableNextRow();
    ComponentUIHelpers::DrawPropertyLabel(label);

    ImGui::TableSetColumnIndex(1);
    std::string checkId = std::string("##") + label;
    if (ImGui::Checkbox(checkId.c_str(), &value)) {
        ComponentUIHelpers::PushInstantUndo(actionManager, !value, value, &value);
    }

    std::string resetId = std::string("##Reset") + label;
    ComponentUIHelpers::DrawPropertyResetButton(resetId.c_str(), value != defaultValue, [&]() {
        bool oldVal = value;
        ComponentUIHelpers::PushInstantUndo(actionManager, oldVal, defaultValue, &value);
    });
}

void GlobalPostProcessComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* pp = dynamic_cast<GlobalPostProcessComponent*>(component);
    if (!pp) {
        return;
    }

    const auto& overrides = pp->GetOverrides();
    int indexToRemove = -1;

    for (size_t i = 0; i < overrides.size(); ++i) {
        auto& setting = overrides[i];

        std::string headerName = setting->GetName();
        std::string icon = "";
        if (setting->GetMode() == PostProcessMode::Bloom) {
            icon = ICON_FA_SUN " ";
        } else if (setting->GetMode() == PostProcessMode::ToneMapping) {
            icon = ICON_FA_PALETTE " ";
        } else if (setting->GetMode() == PostProcessMode::Vignette) {
            icon = ICON_FA_CAMERA " ";
        } else if (setting->GetMode() == PostProcessMode::DepthBasedOutline) {
            icon = ICON_FA_PENCIL " ";
        }

        headerName = icon + headerName + "##" + std::to_string(i);

        bool open = ImGui::CollapsingHeader(headerName.c_str(),
                                            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        // ゴミ箱アイコン（削除ボタン）をヘッダーの右端に配置
        ImGui::SameLine(ImGui::GetWindowWidth() - 40.0f);
        if (ImGui::Button((ICON_FA_TRASH_CAN "##remove_" + std::to_string(i)).c_str())) {
            indexToRemove = static_cast<int>(i);
        }

        if (open) {
            if (ComponentUIHelpers::BeginPropertyTable(("PropTable##" + std::to_string(i)).c_str())) {
                DrawBoolProperty("Enable Override", setting->enabled, true, actionManager);

                ImGui::BeginDisabled(!setting->enabled);

                if (setting->GetMode() == PostProcessMode::Bloom) {
                    auto* bloom = static_cast<BloomSettings*>(setting.get());
                    DrawFloatProperty("Intensity", bloom->intensity, 1.8f, 0.0f, 10.0f, actionManager);
                    DrawFloatProperty("Threshold", bloom->threshold, 0.6f, 0.0f, 10.0f, actionManager);
                    DrawFloatProperty("Sigma", bloom->sigma, 4.0f, 0.1f, 20.0f, actionManager);
                } else if (setting->GetMode() == PostProcessMode::ToneMapping) {
                    auto* cg = static_cast<ColorGradingSettings*>(setting.get());
                    DrawFloatProperty("Exposure", cg->exposure, 1.0f, 0.1f, 10.0f, actionManager);
                    DrawFloatProperty("Hue", cg->hue, 0.0f, -180.0f, 180.0f, actionManager);
                    DrawFloatProperty("Saturation", cg->saturation, 0.0f, -1.0f, 2.0f, actionManager);
                    DrawFloatProperty("Value", cg->value, 0.0f, -1.0f, 2.0f, actionManager);
                } else if (setting->GetMode() == PostProcessMode::Vignette) {
                    auto* vig = static_cast<VignetteSettings*>(setting.get());
                    DrawFloatProperty("Radius", vig->radius, 0.8f, 0.0f, 1.5f, actionManager);
                    DrawFloatProperty("Softness", vig->softness, 0.5f, 0.0f, 1.0f, actionManager);
                } else if (setting->GetMode() == PostProcessMode::DepthBasedOutline) {
                    auto* out = static_cast<OutlineSettings*>(setting.get());
                    DrawFloatProperty("Intensity", out->intensity, 6.0f, 0.0f, 20.0f, actionManager);

                    float r = static_cast<float>(out->maskMaxRadius);
                    DrawFloatProperty("Mask Max Radius", r, 5.0f, 1.0f, 15.0f, actionManager);
                    out->maskMaxRadius = static_cast<int32_t>(r);
                }
                // 追加のエフェクトはここに記述

                ImGui::EndDisabled();
                ComponentUIHelpers::EndPropertyTable();
            }
        }
    }

    if (indexToRemove >= 0) {
        size_t idx = static_cast<size_t>(indexToRemove);
        if (idx < pp->GetOverrides().size()) {
            auto setting = pp->GetOverrides()[idx];
            actionManager->PushAndExecute(std::make_unique<RemoveOverrideCommand>(pp, setting, idx));
        }
    }

    ImGui::Separator();
    ImGui::Spacing();

    // エフェクト追加用のドロップダウン
    const char* effects[] = {"Bloom", "Color Grading", "Vignette", "Outline"};
    static int selectedEffect = 0;

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
    ImGui::Combo("##AddEffectCombo", &selectedEffect, effects, IM_ARRAYSIZE(effects));
    ImGui::SameLine();
    if (ImGui::Button(ICON_FA_PLUS " Add Effect", ImVec2(-1, 0))) {
        PostProcessMode mode = PostProcessMode::None;
        if (selectedEffect == 0) {
            mode = PostProcessMode::Bloom;
        } else if (selectedEffect == 1) {
            mode = PostProcessMode::ToneMapping;
        } else if (selectedEffect == 2) {
            mode = PostProcessMode::Vignette;
        } else if (selectedEffect == 3) {
            mode = PostProcessMode::DepthBasedOutline;
        }

        if (mode != PostProcessMode::None) {
            // 重複チェック
            bool exists = false;
            for (const auto& s : pp->GetOverrides()) {
                if (s->GetMode() == mode) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                auto newSetting = PostProcessSettingsFactory::Create(mode);
                if (newSetting) {
                    size_t insertIndex = pp->GetOverrides().size();
                    actionManager->PushAndExecute(std::make_unique<AddOverrideCommand>(pp, newSetting, insertIndex));
                }
            }
        }
    }
}
#endif // EditorMode
