#include "Panels/HierarchyPanel.h"

#ifdef EditorMode
#include "imgui/imgui.h"
#include "Core/EditorManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "UI/EditorDragDrop.h"
#include "Framework/Scene/SceneSerializer.h"
#include "EngineResources/FontAwesome/IconsFontAwesome6.h"

#include <functional>
#include <algorithm>

void HierarchyPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void HierarchyPanel::Draw() {
    if (!editorManager_) {
        return;
    }

    ImGui::Begin(GetName(), &isOpen_);

    auto* engine = editorManager_->GetEngine();
    if (engine && engine->GetSceneManager()) {
        auto* currentScene = engine->GetSceneManager()->GetCurrentScene();
        auto* baseScene = dynamic_cast<BaseScene*>(currentScene);

        if (baseScene) {
            // 背景クリックなどで選択解除する機能
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                !ImGui::IsAnyItemHovered()) {
                editorManager_->ClearSelectedObject();
            }

            // 循環参照チェック用ラムダ
            auto IsDescendant = [](std::shared_ptr<GameObject> potentialDescendant, GameObject* ancestor) {
                if (!potentialDescendant || !ancestor) {
                    return false;
                }
                auto current = potentialDescendant->GetParent();
                while (current) {
                    if (current.get() == ancestor) {
                        return true;
                    }
                    current = current->GetParent();
                }
                return false;
            };

            // 再帰描画用ラムダ関数
            auto DrawNode = [&](auto& self, const std::shared_ptr<GameObject>& obj) -> void {
                if (!obj || obj->GetHideInHierarchy()) {
                    return;
                }

                bool isSelected = false;
                if (auto selected = editorManager_->GetSelectedObject()) {
                    isSelected = (selected == obj);
                }

                // 描画開始時点での子の有無を保存（途中で子が追加されても不整合を起こさないため）
                bool hasChildrenAtStart = !obj->GetChildren().empty();

                // 子がいればツリーノード、いなければリーフ
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                           ImGuiTreeNodeFlags_SpanAvailWidth;
                if (!hasChildrenAtStart) {
                    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                }
                if (isSelected) {
                    flags |= ImGuiTreeNodeFlags_Selected;
                }

                // IDスタックでチェックボックス名の競合を防ぐ
                ImGui::PushID(obj.get());

                // 識別用にポインタアドレスを使う
                std::string icon = obj->GetIsFolder() ? ICON_FA_FOLDER : ICON_FA_CUBE;
                std::string displayName = icon + " " + obj->GetName();

                static GameObject* renamingObject = nullptr;
                static char renameBuffer[256] = "";
                bool isRenaming = (renamingObject == obj.get());

                bool isOpen = ImGui::TreeNodeEx((void*)obj.get(), flags, "%s",
                                                isRenaming ? (icon + " ").c_str() : displayName.c_str());

                // コンテキストメニュー (右クリック)
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::Selectable("Duplicate")) {
                        if (auto am = editorManager_->GetActionManager()) {
                            am->DuplicateObject(obj);
                        }
                    }
                    if (ImGui::Selectable("Delete")) {
                        if (auto am = editorManager_->GetActionManager()) {
                            am->DeleteObject(obj);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::Selectable("Save as Prefab")) {
                        std::filesystem::create_directories(
                            "resources/prefabs"); // ディレクトリがないと保存に失敗するため
                        std::string path = "resources/prefabs/" + obj->GetName() + ".prefab.json";
                        SceneSerializer::SavePrefab(obj, path);
                    }
                    ImGui::EndPopup();
                }

                // クリックで選択 (TreeNodeExがクリックされたかを判定)
                if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                    editorManager_->SetSelectedObject(obj);
                }

                // ダブルクリックでリネームモードへ
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    renamingObject = obj.get();
                    strncpy_s(renameBuffer, obj->GetName().c_str(), sizeof(renameBuffer) - 1);
                }

                if (isRenaming) {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 65.0f);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer),
                                         ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
                        obj->SetName(renameBuffer);
                        renamingObject = nullptr;
                    } else if (ImGui::IsItemDeactivated()) {
                        obj->SetName(renameBuffer);
                        renamingObject = nullptr;
                    }
                    ImGui::PopStyleVar();
                }

                // 右端にActive切り替えとロック状態のトグルを配置
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.0f);

                // ロックボタン
                bool isLocked = obj->GetIsLocked();
                std::string lockIcon = isLocked ? ICON_FA_LOCK : ICON_FA_UNLOCK;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 背景透明
                ImGui::PushStyleColor(ImGuiCol_Text, isLocked ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f)
                                                              : ImGui::GetStyle().Colors[ImGuiCol_Text]);
                if (ImGui::Button((lockIcon + "##Lock").c_str())) {
                    obj->SetIsLocked(!isLocked);
                }
                ImGui::PopStyleColor(2);
                ImGui::SameLine();

                // 目（可視）ボタン
                bool isActive = obj->GetIsActive();
                std::string eyeIcon = isActive ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0)); // 背景透明
                ImGui::PushStyleColor(ImGuiCol_Text, isActive ? ImGui::GetStyle().Colors[ImGuiCol_Text]
                                                              : ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                if (ImGui::Button((eyeIcon + "##Active").c_str())) {
                    obj->SetIsActive(!isActive);
                }
                ImGui::PopStyleColor(2);

                ImGui::PopID();

                // --- Drag and Drop Source ---
                if (!obj->GetIsLocked() && ImGui::BeginDragDropSource()) {
                    GameObject* ptr = obj.get();
                    ImGui::SetDragDropPayload(EditorDragDrop::PayloadGameObject, &ptr, sizeof(GameObject*));
                    ImGui::Text("Move %s", obj->GetName().c_str());
                    ImGui::EndDragDropSource();
                }

                // --- Drag and Drop Target ---
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadGameObject)) {
                        GameObject* payload_ptr = *(GameObject**)payload->Data;

                        // 自分自身にはDropできない
                        // ドロップ先がロックされていないか
                        // また、ドロップされるオブジェクトが「今の自分の親（先祖）」であってはならない（循環参照の防止）
                        if (!obj->GetIsLocked() && payload_ptr != obj.get() && !IsDescendant(obj, payload_ptr)) {
                            if (auto dropObj = baseScene->FindGameObject(payload_ptr)) {
                                dropObj->SetParent(obj);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // コンテキストメニューは上部に移動しました
                // 子ノードの描画
                if (isOpen && hasChildrenAtStart) {
                    // vector のコピーを回す（描画中に要素が削除・追加されても安全なように）
                    auto childrenCopy = obj->GetChildren();
                    for (auto& child : childrenCopy) {
                        self(self, child);
                    }
                    ImGui::TreePop();
                }
            };

            const auto& gameObjects = baseScene->GetGameObjects();
            auto gameObjectsCopy = gameObjects; // 描画中のリスト変更対策
            for (auto& obj : gameObjectsCopy) {
                // ルートオブジェクトのみを描画開始（子は再帰的に呼ばれる）
                if (obj && !obj->GetParent() && !obj->GetHideInHierarchy()) {
                    DrawNode(DrawNode, obj);
                }
            }

            // --- 余白でのD&D（ルートへ移動 ＆ アセット配置） ---
            ImGui::InvisibleButton("HierarchyDropZone", ImGui::GetContentRegionAvail());
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadGameObject)) {
                    GameObject* payload_ptr = *(GameObject**)payload->Data;
                    if (auto obj = baseScene->FindGameObject(payload_ptr)) {
                        obj->SetParent(nullptr); // 親を解除してルートに
                    }
                }
                // --- アセットのドロップを受け付ける ---
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadAssetPath)) {
                    std::string droppedPathStr = static_cast<const char*>(payload->Data);
                    if (auto am = editorManager_->GetActionManager()) {
                        am->CreateObjectFromAsset(droppedPathStr);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // --- 全体の空白での右クリック「Create」メニューを表示 ---
            if (ImGui::BeginPopupContextItem("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight)) {
                if (auto am = editorManager_->GetActionManager()) {
                    if (ImGui::Selectable("Create Empty")) {
                        am->CreatePrimitiveObject("Empty");
                    }
                    if (ImGui::Selectable("Create Folder")) {
                        am->CreatePrimitiveObject("Empty");
                        if (auto newObj = editorManager_->GetSelectedObject()) {
                            newObj->SetName("New Folder");
                            newObj->SetIsFolder(true);
                        }
                    }

                    if (ImGui::BeginMenu("3D Object")) {
                        if (ImGui::Selectable("Cube")) {
                            am->CreatePrimitiveObject("Cube");
                        }
                        if (ImGui::Selectable("Sphere")) {
                            am->CreatePrimitiveObject("Sphere");
                        }
                        if (ImGui::Selectable("Cylinder")) {
                            am->CreatePrimitiveObject("Cylinder");
                        }
                        if (ImGui::Selectable("Plane")) {
                            am->CreatePrimitiveObject("Plane");
                        }
                        ImGui::Separator();
                        if (ImGui::Selectable("Model (MeshRenderer)")) {
                            am->CreatePrimitiveObject("Model");
                        }
                        ImGui::EndMenu();
                    }

                    if (ImGui::BeginMenu("2D Object")) {
                        if (ImGui::Selectable("Sprite")) {
                            am->CreatePrimitiveObject("Sprite");
                        }
                        ImGui::EndMenu();
                    }
                }
                ImGui::EndPopup();
            }
            // --- Deleteキーでの削除ロジックは EditorShortcutManager に移譲したため削除 ---
        }
    }

    ImGui::End();
}
#endif // EditorMode
