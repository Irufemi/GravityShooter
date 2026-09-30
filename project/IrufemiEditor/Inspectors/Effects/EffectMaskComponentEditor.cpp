#include "Inspectors/Effects/EffectMaskComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "UI/ComponentUIHelpers.h"

void EffectMaskComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* maskComp = static_cast<EffectMaskComponent*>(component);
    if (!maskComp) {
        return;
    }

    bool headerOpen = ImGui::TreeNodeEx("EffectMask", ImGuiTreeNodeFlags_DefaultOpen);

    bool pendingRemove = false;
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Remove Component")) {
            pendingRemove = true;
        }
        ImGui::EndPopup();
    }

    if (headerOpen) {
        if (ComponentUIHelpers::BeginPropertyTable("EffectMaskTable")) {
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Enable Mask", "Protects the object from post-process effects.");
            ImGui::TableSetColumnIndex(1);

            bool enable = maskComp->GetEnableEffectMask();
            if (ImGui::Checkbox("##EnableEffectMask", &enable)) {
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<bool>>(
                    maskComp->GetEnableEffectMask(), enable,
                    [maskComp](const bool& val) { maskComp->SetEnableEffectMask(val); }));
            }

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Custom Effect", "Type of individual effect to apply");
            ImGui::TableSetColumnIndex(1);

            const char* effectTypes[] = {"None",
                                         "Grayscale",
                                         "Sepia",
                                         "Vignette",
                                         "Smoothing",
                                         "GaussianFilter",
                                         "DepthBasedOutline",
                                         "RadialBlur",
                                         "Dissolve",
                                         "Noise",
                                         "HSV",
                                         "ToneMapping",
                                         "Fade",
                                         "Slide",
                                         "Bloom",
                                         "Glitch",
                                         "DualKawaseBlur",
                                         "LuminanceBasedOutline",
                                         "Pixelation",
                                         "Pointillism",
                                         "Posterization",
                                         "NightVision",
                                         "Kaleidoscope",
                                         "ChromaticAberration",
                                         "DisplacementMap",
                                         "DirectionalBlur",
                                         "Halftone",
                                         "DepthOfField",
                                         "LightShafts"};

            int type = maskComp->GetCustomEffectType();
            if (ImGui::Combo("##CustomEffectType", &type, effectTypes, IM_ARRAYSIZE(effectTypes))) {
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<int32_t>>(
                    maskComp->GetCustomEffectType(), type,
                    [maskComp](const int32_t& val) { maskComp->SetCustomEffectType(val); }));
            }

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Effect Params", "Detailed parameters");
            ImGui::TableSetColumnIndex(1);

            auto params = maskComp->GetCustomParams();
            static PostProcessManager::CustomEffectParams startParams;
            static bool isDragging = false;
            bool changed = false;

            auto checkActive = [&]() {
                if (ImGui::IsItemActivated()) {
                    startParams = maskComp->GetCustomParams();
                    isDragging = true;
                }
            };

            if (type == static_cast<int>(PostProcessMode::Dissolve)) {
                if (ImGui::ColorEdit4("Edge Color", &params.color1.x)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::ColorEdit4("Bg Color", &params.color2.x)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Threshold", &params.param1, 0.01f, 0.0f, 1.0f)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Edge Range", &params.param2, 0.001f, 0.0f, 1.0f)) {
                    changed = true;
                }
                checkActive();
                int noiseType = static_cast<int>(params.param3);
                if (ImGui::Combo("Noise Type", &noiseType, "Type 0\0Type 1\0")) {
                    params.param3 = static_cast<float>(noiseType);
                    changed = true;
                }
                checkActive();
            } else if (type == 15) { // Glitch
                if (ImGui::DragFloat("Intensity", &params.param1, 0.01f, 0.0f, 10.0f)) {
                    changed = true;
                }
                checkActive();
            } else if (type == 13) { // Slide
                if (ImGui::ColorEdit4("Color", &params.color1.x)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Threshold", &params.param1, 0.01f, 0.0f, 1.0f)) {
                    changed = true;
                }
                checkActive();
            } else if (type == 12) { // Fade
                if (ImGui::ColorEdit4("Color", &params.color1.x)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Intensity", &params.param1, 0.01f, 0.0f, 1.0f)) {
                    changed = true;
                }
                checkActive();
            } else if (type == 3) { // Vignette
                if (ImGui::ColorEdit4("Color", &params.color1.x)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Radius", &params.param1, 0.01f, 0.0f, 2.0f)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Softness", &params.param2, 0.01f, 0.0f, 2.0f)) {
                    changed = true;
                }
                checkActive();
            } else if (type == 6 || type == 17) { // Outlines
                if (ImGui::ColorEdit4("Color", &params.color1.x)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Weight", &params.param1, 0.01f, 0.0f, 10.0f)) {
                    changed = true;
                }
                checkActive();
            } else if (type > 0) { // Fallback for other effects
                if (ImGui::DragFloat("Param 1", &params.param1, 0.01f, 0.0f, 10.0f)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::DragFloat("Param 2", &params.param2, 0.01f, 0.0f, 10.0f)) {
                    changed = true;
                }
                checkActive();
                if (ImGui::ColorEdit4("Color 1", &params.color1.x)) {
                    changed = true;
                }
                checkActive();
            }

            if (changed) {
                maskComp->GetCustomParams() = params;
            }

            if (ImGui::IsItemDeactivatedAfterEdit() || (isDragging && !ImGui::IsAnyItemActive())) {
                if (isDragging) {
                    actionManager->PushAndExecute(
                        std::make_unique<ChangeValueCommand<PostProcessManager::CustomEffectParams>>(
                            startParams, maskComp->GetCustomParams(),
                            [maskComp](const PostProcessManager::CustomEffectParams& val) {
                                maskComp->GetCustomParams() = val;
                            }));
                    isDragging = false;
                }
            }

            ComponentUIHelpers::EndPropertyTable();
        }
        ImGui::TreePop();
    }

    if (pendingRemove) {
        actionManager->PushAndExecute(std::make_unique<RemoveComponentCommand>(
            maskComp->GetGameObject()->shared_from_this(),
            ComponentUIHelpers::GetSharedComponent(maskComp->GetGameObject(), maskComp)));
    }
}
#endif // EditorMode
