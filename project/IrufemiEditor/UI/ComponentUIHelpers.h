#pragma once

#ifdef EditorMode
#include <memory>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include "imgui/imgui.h"
#include "Framework/Component/Component.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"

namespace Irufemi::Editor::UI {

std::shared_ptr<Component> GetSharedComponent(GameObject* go, Component* comp);
void SwitchColliderType(GameObject* go, ColliderComponent* oldComp, ColliderComponent::ColliderType newType,
                        EditorActionManager* actionManager);

template <typename T> void CheckUndoRedoDrag(EditorActionManager* actionManager, T* valuePtr) {
    if (!actionManager || !valuePtr) {
        return;
    }
    static std::unordered_map<const void*, T> activeSessions;

    if (ImGui::IsItemActivated()) {
        activeSessions[valuePtr] = *valuePtr;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = activeSessions.find(valuePtr);
        if (it != activeSessions.end()) {
            T startValue = it->second;
            T endValue = *valuePtr;
            activeSessions.erase(it);
            actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(
                startValue, endValue, [valuePtr](const T& v) { *valuePtr = v; }));
        }
    }
}

template <typename T, typename Func>
void CheckUndoRedoDrag(EditorActionManager* actionManager, T* valuePtr, Func&& callback) {
    if (!actionManager || !valuePtr) {
        return;
    }
    static std::unordered_map<const void*, T> activeSessions;

    if (ImGui::IsItemActivated()) {
        activeSessions[valuePtr] = *valuePtr;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        auto it = activeSessions.find(valuePtr);
        if (it != activeSessions.end()) {
            T startValue = it->second;
            T endValue = *valuePtr;
            activeSessions.erase(it);
            if constexpr (std::is_invocable_v<Func, const T&>) {
                actionManager->PushAndExecute(
                    std::make_unique<ChangeValueCommand<T>>(startValue, endValue, std::forward<Func>(callback)));
            } else if constexpr (std::is_invocable_v<Func>) {
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(
                    startValue, endValue,
                    [valuePtr, cb = std::function<void()>(std::forward<Func>(callback))](const T& v) {
                        *valuePtr = v;
                        if (cb) {
                            cb();
                        }
                    }));
            }
        }
    }
}

template <typename T>
void PushInstantUndo(EditorActionManager* actionManager, const T& oldVal, const T& newVal, T* valuePtr,
                     std::function<void()> onChanged = nullptr) {
    actionManager->PushAndExecute(
        std::make_unique<ChangeValueCommand<T>>(oldVal, newVal, [valuePtr, onChanged](const T& v) {
            *valuePtr = v;
            if (onChanged) {
                onChanged();
            }
        }));
}

template <typename T>
void PushInstantUndo(EditorActionManager* actionManager, const T& oldVal, const T& newVal,
                     std::function<void(const T&)> setter) {
    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<T>>(oldVal, newVal, setter));
}

/**
 * @brief AAA基準の3カラム（名前、値、リセット）プロパティテーブルを開始する
 * @param tableId テーブルの固有ID（デフォルトは "PropertiesTable"）
 * @return テーブルの構築に成功した場合は true
 */
bool BeginPropertyTable(const char* tableId = "PropertiesTable");

/**
 * @brief プロパティテーブルの描画を終了する
 */
void EndPropertyTable();

/**
 * @brief テーブルの第1カラムにプロパティのラベル（名前）を描画する
 * @param label 表示するプロパティ名
 * @param tooltip ホバー時に表示する説明文（省略可）
 */
void DrawPropertyLabel(const char* label, const char* tooltip = nullptr);

/**
 * @brief テーブルの第3カラムにリセットボタン（↺）を描画する
 * @param id ImGui用の固有ID（"##"から始めること）
 * @param isModified 値がデフォルトから変更されているか（trueならボタンが出現）
 * @param resetAction リセットボタンが押された際に実行される処理
 */
void DrawPropertyResetButton(const char* id, bool isModified, std::function<void()> resetAction);

void DrawCollisionLayerGUI(Component* comp, EditorActionManager* actionManager, uint32_t& layer,
                           uint32_t& mask);

template <typename T> void DrawColliderCommonProperties(T* comp, EditorActionManager* actionManager) {
    if (BeginPropertyTable("ColliderProperties")) {
        // Collider Type Switcher
        ColliderComponent::ColliderType currentType = comp->GetColliderType();
        int currentIdx = 0;
        if (currentType == ColliderComponent::ColliderType::AABB) {
            currentIdx = 0;
        } else if (currentType == ColliderComponent::ColliderType::Sphere) {
            currentIdx = 1;
        } else if (currentType == ColliderComponent::ColliderType::OBB) {
            currentIdx = 2;
        }

        const char* typeNames[] = {"Box (AABB)", "Sphere", "Box (OBB)"};
        ImGui::TableNextRow();
        DrawPropertyLabel("Collider Type");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1);
        int selectedIdx = currentIdx;
        if (ImGui::Combo("##ColliderType", &selectedIdx, typeNames, IM_ARRAYSIZE(typeNames))) {
            if (selectedIdx != currentIdx) {
                ColliderComponent::ColliderType newType = ColliderComponent::ColliderType::Sphere;
                if (selectedIdx == 0) {
                    newType = ColliderComponent::ColliderType::AABB;
                } else if (selectedIdx == 1) {
                    newType = ColliderComponent::ColliderType::Sphere;
                } else if (selectedIdx == 2) {
                    newType = ColliderComponent::ColliderType::OBB;
                }
                SwitchColliderType(comp->GetGameObject(), comp, newType, actionManager);
                ImGui::PopItemWidth();
                EndPropertyTable();
                return; // 置換後はコンポーネントが無効になるため即座にリターン
            }
        }
        ImGui::PopItemWidth();

        Irufemi::Vector3 offset = comp->GetLocalOffset();
        ImGui::TableNextRow();
        DrawPropertyLabel("Offset");
        ImGui::TableSetColumnIndex(1);
        ImGui::PushItemWidth(-1);
        if (ImGui::DragFloat3("##Offset", &offset.x, 0.1f)) {
            comp->SetLocalOffset(offset);
        }
        ImGui::PopItemWidth();
        CheckUndoRedoDrag(actionManager, &offset,
                          std::function<void(const Irufemi::Vector3&)>(
                              [comp](const Irufemi::Vector3& v) { comp->SetLocalOffset(v); }));
        DrawPropertyResetButton("##OffsetReset", offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f, [&]() {
            Irufemi::Vector3 oldO = comp->GetLocalOffset();
            PushInstantUndo(actionManager, oldO, Irufemi::Vector3{0, 0, 0},
                            std::function<void(const Irufemi::Vector3&)>(
                                [comp](const Irufemi::Vector3& v) { comp->SetLocalOffset(v); }));
        });

        if constexpr (std::is_same_v<T, SphereColliderComponent>) {
            float radius = comp->GetLocalRadius();
            ImGui::TableNextRow();
            DrawPropertyLabel("Radius");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Radius", &radius, 0.1f, 0.0f, 1000.0f)) {
                comp->SetLocalRadius(radius);
            }
            ImGui::PopItemWidth();
            CheckUndoRedoDrag(actionManager, &radius, std::function<void(const float&)>([comp](const float& v) {
                                  comp->SetLocalRadius(v);
                              }));
            DrawPropertyResetButton("##RadiusReset", radius != 1.0f, [&]() {
                float oldR = comp->GetLocalRadius();
                PushInstantUndo(
                    actionManager, oldR, 1.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetLocalRadius(v); }));
            });
        } else {
            Irufemi::Vector3 size = comp->GetLocalSize();
            ImGui::TableNextRow();
            DrawPropertyLabel("Size (Extents)");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##Size", &size.x, 0.1f, 0.0f, 1000.0f)) {
                comp->SetLocalSize(size);
            }
            ImGui::PopItemWidth();
            CheckUndoRedoDrag(actionManager, &size,
                              std::function<void(const Irufemi::Vector3&)>(
                                  [comp](const Irufemi::Vector3& v) { comp->SetLocalSize(v); }));
            DrawPropertyResetButton("##SizeReset", size.x != 1.0f || size.y != 1.0f || size.z != 1.0f, [&]() {
                Irufemi::Vector3 oldS = comp->GetLocalSize();
                PushInstantUndo(actionManager, oldS, Irufemi::Vector3{1, 1, 1},
                                std::function<void(const Irufemi::Vector3&)>(
                                    [comp](const Irufemi::Vector3& v) { comp->SetLocalSize(v); }));
            });
        }

        bool isTrigger = comp->IsTrigger();
        ImGui::TableNextRow();
        DrawPropertyLabel("Is Trigger");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Checkbox("##Is Trigger", &isTrigger)) {
            PushInstantUndo(actionManager, comp->IsTrigger(), isTrigger,
                            std::function<void(const bool&)>([comp](const bool& v) { comp->SetTrigger(v); }));
        }
        DrawPropertyResetButton("##TriggerReset", isTrigger, [&]() {
            bool oldT = comp->IsTrigger();
            PushInstantUndo(actionManager, oldT, false,
                            std::function<void(const bool&)>([comp](const bool& v) { comp->SetTrigger(v); }));
        });

        EndPropertyTable();
    }

    uint32_t layer = comp->GetLayer();
    uint32_t mask = comp->GetMask();
    DrawCollisionLayerGUI(comp, actionManager, layer, mask);
    comp->SetLayer(layer);
    comp->SetMask(mask);
}

void DrawFallbackPropertiesGUI(Component* component, EditorActionManager* actionManager);

} // namespace Irufemi::Editor::UI

// 後方互換性エイリアス（既存の ComponentUIHelpers:: 呼び出しを 100% 維持）
namespace ComponentUIHelpers = Irufemi::Editor::UI;

#endif // EditorMode
