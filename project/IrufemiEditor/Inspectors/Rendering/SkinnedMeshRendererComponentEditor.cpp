#include "Inspectors/Rendering/SkinnedMeshRendererComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include <string>
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "UI/ComponentUIHelpers.h"

using namespace Irufemi;

void SkinnedMeshRendererComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto comp = static_cast<SkinnedMeshRendererComponent*>(component);
    if (!comp) {
        return;
    }

    bool headerOpen = ImGui::TreeNodeEx("SkinnedMeshRenderer", ImGuiTreeNodeFlags_DefaultOpen);

    bool pendingRemove = false;
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Remove Component")) {
            pendingRemove = true;
        }
        ImGui::EndPopup();
    }
    if (pendingRemove) {
        actionManager->PushAndExecute(std::make_unique<RemoveComponentCommand>(
            comp->GetGameObject()->shared_from_this(),
            ComponentUIHelpers::GetSharedComponent(comp->GetGameObject(), comp)));
        return;
    }

    if (headerOpen) {
        if (auto rawObj = comp->GetRawObject()) {
            size_t meshCount = rawObj->GetMeshCount();

            if (ImGui::CollapsingHeader("Materials (Slots)", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (size_t i = 0; i < meshCount; ++i) {
                    ImGui::PushID(static_cast<int>(i));

                    std::string slotName = "Element " + std::to_string(i);
                    if (ImGui::TreeNode(slotName.c_str())) {
                        bool hasOverride = comp->HasMaterialOverride(i);
                        bool wasOverride = hasOverride;

                        if (ImGui::Checkbox("Override Material", &hasOverride)) {
                            ObjMaterial initialMat;
                            if (hasOverride) {
                                if (const ObjMaterial* original = rawObj->GetMaterial(i)) {
                                    initialMat = *original;
                                }
                            }
                            ObjMaterial oldMat;
                            if (!hasOverride && wasOverride) {
                                if (const ObjMaterial* cur = comp->GetMaterialOverride(i)) {
                                    oldMat = *cur;
                                }
                            }

                            auto setter = [comp, i, mat = (hasOverride ? initialMat : oldMat)](const bool& enabled) {
                                if (enabled) {
                                    comp->SetMaterialOverride(i, mat);
                                } else {
                                    comp->RemoveMaterialOverride(i);
                                }
                            };

                            ComponentUIHelpers::PushInstantUndo(actionManager, wasOverride, hasOverride,
                                                                std::function<void(const bool&)>(setter));
                        }

                        if (comp->HasMaterialOverride(i)) {
                            ObjMaterial* overMat = comp->GetMaterialOverrideMutable(i);
                            if (overMat) {
                                ImGui::ColorEdit4("Color", &overMat->color.x);
                                ComponentUIHelpers::CheckUndoRedoDrag(
                                    actionManager, &overMat->color,
                                    std::function<void(const Irufemi::Vector4&)>([comp, i](const Irufemi::Vector4& v) {
                                        if (ObjMaterial* m = comp->GetMaterialOverrideMutable(i)) {
                                            m->color = v;
                                        }
                                    }));

                                ImGui::SliderFloat("Roughness", &overMat->roughness, 0.0f, 1.0f);
                                ComponentUIHelpers::CheckUndoRedoDrag(
                                    actionManager, &overMat->roughness,
                                    std::function<void(const float&)>([comp, i](const float& v) {
                                        if (ObjMaterial* m = comp->GetMaterialOverrideMutable(i)) {
                                            m->roughness = v;
                                        }
                                    }));

                                ImGui::SliderFloat("Metallic", &overMat->metallic, 0.0f, 1.0f);
                                ComponentUIHelpers::CheckUndoRedoDrag(
                                    actionManager, &overMat->metallic,
                                    std::function<void(const float&)>([comp, i](const float& v) {
                                        if (ObjMaterial* m = comp->GetMaterialOverrideMutable(i)) {
                                            m->metallic = v;
                                        }
                                    }));

                                bool enableLighting = overMat->enableLighting;
                                if (ImGui::Checkbox("Enable Lighting", &enableLighting)) {
                                    bool oldVal = overMat->enableLighting;
                                    ComponentUIHelpers::PushInstantUndo(
                                        actionManager, oldVal, enableLighting,
                                        std::function<void(const bool&)>([comp, i](const bool& v) {
                                            if (ObjMaterial* m = comp->GetMaterialOverrideMutable(i)) {
                                                m->enableLighting = v;
                                            }
                                        }));
                                }
                            }
                        }

                        ImGui::TreePop();
                    }

                    ImGui::PopID();
                }
            }
        } else {
            ImGui::TextDisabled("Model not loaded yet.");
        }

        ImGui::TreePop();
    }
}
#endif // EditorMode
