#include "Inspectors/Physics/ColliderComponentEditors.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "UI/ComponentUIHelpers.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"

using namespace Irufemi;

void AABBColliderComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<AABBColliderComponent*>(component);
    ImGui::PushID(comp);
    bool headerOpen = ImGui::CollapsingHeader("AABB Collider", ImGuiTreeNodeFlags_DefaultOpen);

    if (ComponentUIHelpers::DrawComponentContextMenu(comp, actionManager)) {
        ImGui::PopID();
        return;
    }

    if (headerOpen) {
        ComponentUIHelpers::DrawColliderCommonProperties(comp, actionManager);
    }
    ImGui::PopID();
}

void OBBColliderComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<OBBColliderComponent*>(component);
    ImGui::PushID(comp);
    bool headerOpen = ImGui::CollapsingHeader("OBB Collider", ImGuiTreeNodeFlags_DefaultOpen);

    if (ComponentUIHelpers::DrawComponentContextMenu(comp, actionManager)) {
        ImGui::PopID();
        return;
    }

    if (headerOpen) {
        ComponentUIHelpers::DrawColliderCommonProperties(comp, actionManager);
    }
    ImGui::PopID();
}

void SphereColliderComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<SphereColliderComponent*>(component);
    ImGui::PushID(comp);
    bool headerOpen = ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen);

    if (ComponentUIHelpers::DrawComponentContextMenu(comp, actionManager)) {
        ImGui::PopID();
        return;
    }

    if (headerOpen) {
        ComponentUIHelpers::DrawColliderCommonProperties(comp, actionManager);
    }
    ImGui::PopID();
}

#endif // EditorMode
