#include "Inspectors/Rendering/ModelBatchRendererComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include <filesystem>
#include "UI/ComponentUIHelpers.h"
#include "UI/EditorDragDrop.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "Core/System/IrufemiEngine.h"
#include "Resource/Model/ModelManager.h"

void ModelBatchRendererComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<ModelBatchRendererComponent*>(component);
    bool headerOpen = ImGui::TreeNodeEx("ModelBatchRenderer", ImGuiTreeNodeFlags_DefaultOpen);

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
    }

    if (headerOpen) {
        if (ComponentUIHelpers::BeginPropertyTable("ModelBatchRendererTable")) {
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Model", "Drag & Drop model file from Project Browser");

            ImGui::TableSetColumnIndex(1);
            IrufemiEngine* engine = nullptr;
            if (comp->GetGameObject() && comp->GetGameObject()->GetScene()) {
                engine = comp->GetGameObject()->GetScene()->GetEngine();
            }

            if (engine && engine->GetObjModelManager()) {
                ModelManager* modelManager = engine->GetObjModelManager();
                std::vector<std::string> availableModels = modelManager->GetAvailableModels();

                if (std::find(availableModels.begin(), availableModels.end(), comp->GetModelName()) ==
                    availableModels.end()) {
                    availableModels.push_back(comp->GetModelName());
                }

                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
                if (ImGui::Button("Refresh")) {
                    modelManager->RefreshAvailableModels();
                    availableModels = modelManager->GetAvailableModels();
                    if (std::find(availableModels.begin(), availableModels.end(), comp->GetModelName()) ==
                        availableModels.end()) {
                        availableModels.push_back(comp->GetModelName());
                    }
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##ModelCombo", comp->GetModelName().c_str())) {
                    for (const auto& key : availableModels) {
                        bool isSelected = (comp->GetModelName() == key);
                        if (ImGui::Selectable(key.c_str(), isSelected)) {
                            std::string oldModel = comp->GetModelName();
                            std::string newModel = key;
                            ComponentUIHelpers::PushInstantUndo(
                                actionManager, oldModel, newModel,
                                std::function<void(const std::string&)>(
                                    [comp](const std::string& v) { comp->LoadModel(v); }));
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopStyleVar();
            } else {
                ImGui::Text("%s", comp->GetModelName().c_str());
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadAssetPath)) {
                    std::string droppedPathStr = static_cast<const char*>(payload->Data);
                    std::filesystem::path droppedPath(reinterpret_cast<const char8_t*>(droppedPathStr.c_str()));
                    std::string ext = droppedPath.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".obj" || ext == ".gltf" || ext == ".fbx" || ext == ".glb") {
                        std::string newModelName = droppedPathStr;
                        std::replace(newModelName.begin(), newModelName.end(), '\\', '/');
                        std::string lowerPath = newModelName;
                        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
                        if (lowerPath.find("resources/model/") == 0) {
                            newModelName = newModelName.substr(16);
                        }
                        std::string oldModel = comp->GetModelName();
                        ComponentUIHelpers::PushInstantUndo(actionManager, oldModel, newModelName,
                                                            std::function<void(const std::string&)>(
                                                                [comp](const std::string& v) { comp->LoadModel(v); }));
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ComponentUIHelpers::DrawPropertyResetButton("##ModelReset", !comp->GetModelName().empty(), [&]() {
                std::string oldModel = comp->GetModelName();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldModel, std::string(""),
                    std::function<void(const std::string&)>([comp](const std::string& v) { comp->LoadModel(v); }));
            });

            ComponentUIHelpers::EndPropertyTable();
        }
        ImGui::TreePop();
    }
}
#endif // EditorMode
