#include "Inspectors/Transform/TransformComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "UI/ComponentUIHelpers.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"

void TransformComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<TransformComponent*>(component);
    bool headerOpen = ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen);

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Reset")) {
            Irufemi::Vector3 oldPos = comp->GetPosition();
            Irufemi::Vector3 oldRot = comp->GetRotation();
            Irufemi::Vector3 oldScale = comp->GetScale();
            if (oldPos.x != 0.0f || oldPos.y != 0.0f || oldPos.z != 0.0f) {
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                    oldPos, Irufemi::Vector3{0, 0, 0}, [comp](const Irufemi::Vector3& v) { comp->SetPosition(v); }));
            }
            if (oldRot.x != 0.0f || oldRot.y != 0.0f || oldRot.z != 0.0f) {
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                    oldRot, Irufemi::Vector3{0, 0, 0}, [comp](const Irufemi::Vector3& v) { comp->SetRotation(v); }));
            }
            if (oldScale.x != 1.0f || oldScale.y != 1.0f || oldScale.z != 1.0f) {
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                    oldScale, Irufemi::Vector3{1, 1, 1}, [comp](const Irufemi::Vector3& v) { comp->SetScale(v); }));
            }
        }
        ImGui::Separator();
        ImGui::MenuItem("Remove Component", nullptr, false, false);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Transform component cannot be removed.");
        }
        ImGui::EndPopup();
    }

    if (headerOpen) {
        if (ComponentUIHelpers::BeginPropertyTable("TransformTable")) {
            // Position
            Irufemi::Vector3 pos = comp->GetPosition();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Position");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##Position", &pos.x, 0.1f)) {
                comp->SetPosition(pos);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &pos,
                                                  [comp](const Irufemi::Vector3& v) { comp->SetPosition(v); });
            ComponentUIHelpers::DrawPropertyResetButton(
                "##Pos", pos.x != 0.0f || pos.y != 0.0f || pos.z != 0.0f, [&]() {
                    Irufemi::Vector3 oldPos = comp->GetPosition();
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                        oldPos, Irufemi::Vector3{0, 0, 0},
                        [comp](const Irufemi::Vector3& v) { comp->SetPosition(v); }));
                });

            // Rotation
            Irufemi::Vector3 rot = comp->GetRotation();
            Irufemi::Vector3 rotDegrees = {rot.x * (180.0f / 3.14159265f), rot.y * (180.0f / 3.14159265f),
                                           rot.z * (180.0f / 3.14159265f)};
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Rotation");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##Rotation", &rotDegrees.x, 1.0f)) {
                rot.x = rotDegrees.x * (3.14159265f / 180.0f);
                rot.y = rotDegrees.y * (3.14159265f / 180.0f);
                rot.z = rotDegrees.z * (3.14159265f / 180.0f);
                comp->SetRotation(rot);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &rot,
                                                  [comp](const Irufemi::Vector3& v) { comp->SetRotation(v); });
            ComponentUIHelpers::DrawPropertyResetButton(
                "##Rot", rot.x != 0.0f || rot.y != 0.0f || rot.z != 0.0f, [&]() {
                    Irufemi::Vector3 oldRot = comp->GetRotation();
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                        oldRot, Irufemi::Vector3{0, 0, 0},
                        [comp](const Irufemi::Vector3& v) { comp->SetRotation(v); }));
                });

            // Scale
            Irufemi::Vector3 scale = comp->GetScale();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Scale");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##Scale", &scale.x, 0.1f)) {
                comp->SetScale(scale);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &scale,
                                                  [comp](const Irufemi::Vector3& v) { comp->SetScale(v); });
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ScaleBtn", scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f, [&]() {
                    Irufemi::Vector3 oldScale = comp->GetScale();
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3>>(
                        oldScale, Irufemi::Vector3{1, 1, 1},
                        [comp](const Irufemi::Vector3& v) { comp->SetScale(v); }));
                });

            ComponentUIHelpers::EndPropertyTable();
        }

        ImGui::TreePop();
    }
}
#endif // EditorMode
