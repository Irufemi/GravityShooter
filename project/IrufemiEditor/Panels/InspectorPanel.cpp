#include "Panels/InspectorPanel.h"

#ifdef EditorMode
#include "imgui/imgui.h"
#include "Core/EditorManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/SceneManager.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/EditorTheme.h"
#include "Inspectors/ComponentEditorRegistry.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "Framework/Component/ComponentFactory.h"
#include <unordered_set>
#include <map>
#include <cstring>

void InspectorPanel::Initialize(EditorManager* editorManager) {
    editorManager_ = editorManager;
}

void InspectorPanel::Draw() {
    if (!editorManager_) {
        return;
    }

    ImGui::Begin(GetName(), &isOpen_);

    if (auto selected = editorManager_->GetSelectedObject()) {
        bool isLocked = selected->GetIsLocked();
        if (isLocked) {
            ImGui::BeginDisabled();
        }

        char nameBuffer[256];
        strncpy_s(nameBuffer, selected->GetName().c_str(), sizeof(nameBuffer) - 1);

        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 150); // Deleteボタンのスペースを確保
        static std::string startName;
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            selected->SetName(nameBuffer);
        }
        if (ImGui::IsItemActivated()) {
            startName = selected->GetName();
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            std::string endName = selected->GetName();
            editorManager_->GetActionManager()->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                startName, endName, [selected](const std::string& s) { selected->SetName(s); }));
        }

        // --- オブジェクト削除ボタン（赤色で右端に配置） ---
        ImGui::SameLine(ImGui::GetWindowWidth() - 80);
        {
            EditorTheme::ScopedDangerButton dangerStyle;
            if (ImGui::Button("Delete", ImVec2(70, 0))) {
                if (auto actionManager = editorManager_->GetActionManager()) {
                    actionManager->DeleteObject(selected);
                }
            }
        }
        // ------------------------------------------------

        ImGui::Separator();

        // コンポーネントのUIを描画
        if (auto sel = editorManager_->GetSelectedObject()) {
            auto* actionManager = editorManager_->GetActionManager();
            if (auto registry = editorManager_->GetComponentEditorRegistry()) {
                for (const auto& comp : sel->GetComponents()) {
                    registry->DrawComponent(comp.get(), actionManager);
                }
            }

            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
                ImGui::OpenPopup("AddComponentPopup");
            }

            if (ImGui::BeginPopup("AddComponentPopup")) {
                bool hasTransform = false;
                bool hasAnyRenderer = false;
                std::unordered_set<std::string> existingComponents;

                for (const auto& comp : sel->GetComponents()) {
                    if (!comp) {
                        continue;
                    }
                    std::string name = comp->GetComponentName();
                    existingComponents.insert(name);

                    if (name == "TransformComponent") {
                        hasTransform = true;
                    }
                    if (name.find("Renderer") != std::string::npos) {
                        hasAnyRenderer = true;
                    }
                }

                // カテゴリ分類（コンポーネント登録数の変化を検知してキャッシュ再構築）
                static std::map<std::string, std::vector<std::string>> s_categoriesCache;
                static size_t s_lastFactoryMapSize = 0;

                const auto& factoryMap = ComponentFactory::GetFactoryMap();
                if (s_lastFactoryMapSize != factoryMap.size()) {
                    s_categoriesCache.clear();
                    for (const auto& [name, reg] : factoryMap) {
                        if (name == "TransformComponent") {
                            continue; // 個別処理
                        }
                        s_categoriesCache[reg.category].push_back(name);
                    }
                    s_lastFactoryMapSize = factoryMap.size();
                }
                const auto& categories = s_categoriesCache;

                if (!hasTransform) {
                    if (ImGui::Selectable("TransformComponent")) {
                        auto comp = ComponentFactory::Create("TransformComponent");
                        actionManager->PushAndExecute(std::make_unique<AddComponentCommand>(sel, comp));
                    }
                } else {
                    ImGui::TextDisabled("TransformComponent (Already added)");
                }

                ImGui::Separator();

                // カテゴリごとにメニューを描画
                for (const auto& [category, names] : categories) {
                    if (ImGui::BeginMenu(category.c_str())) {
                        bool isRendererCat = (category == "Renderer");

                        if (isRendererCat && hasAnyRenderer) {
                            ImGui::TextDisabled("Only one renderer is allowed.");
                            ImGui::Separator();
                        }

                        for (const auto& compName : names) {
                            bool alreadyAdded = existingComponents.find(compName) != existingComponents.end();

                            if (alreadyAdded) {
                                ImGui::TextDisabled("%s (Already added)", compName.c_str());
                            } else if (isRendererCat && hasAnyRenderer) {
                                ImGui::TextDisabled("%s", compName.c_str()); // レンダラー重複制限
                            } else {
                                if (ImGui::Selectable(compName.c_str())) {
                                    auto comp = ComponentFactory::Create(compName);
                                    actionManager->PushAndExecute(std::make_unique<AddComponentCommand>(sel, comp));
                                }
                            }
                        }
                        ImGui::EndMenu();
                    }
                }

                ImGui::Separator();

                if (ImGui::BeginMenu("Remove Component")) {
                    bool hasRemovable = false;
                    for (auto& comp : sel->GetComponents()) {
                        if (!comp) {
                            continue;
                        }
                        std::string compName = comp->GetComponentName();
                        if (compName == "TransformComponent") {
                            continue; // Transformは削除不可
                        }

                        hasRemovable = true;
                        if (ImGui::Selectable(compName.c_str())) {
                            actionManager->PushAndExecute(std::make_unique<RemoveComponentCommand>(sel, comp));
                            ImGui::EndMenu();
                            ImGui::EndPopup();
                            break;
                        }
                    }
                    if (!hasRemovable) {
                        ImGui::TextDisabled("No removable components.");
                    } else {
                        ImGui::EndMenu();
                    }
                } else {
                    // Remove Component menu didn't begin, so we do nothing here
                }

                ImGui::EndPopup();
            }
        }

        if (isLocked) {
            ImGui::EndDisabled();
        }
    } else {
        ImGui::Text("No object selected.");
    }

    ImGui::End();
}
#endif // EditorMode
