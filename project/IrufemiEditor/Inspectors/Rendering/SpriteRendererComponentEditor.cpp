#include "Inspectors/Rendering/SpriteRendererComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include <filesystem>
#include <algorithm>
#include "UI/ComponentUIHelpers.h"
#include "Framework/Component/Renderer/SpriteRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "UI/EditorDragDrop.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/System/IrufemiEngine.h"

void SpriteRendererComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<SpriteRendererComponent*>(component);
    bool headerOpen = ImGui::TreeNodeEx("SpriteRenderer", ImGuiTreeNodeFlags_DefaultOpen);

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
        if (ComponentUIHelpers::BeginPropertyTable("SpriteRendererTable")) {
            TextureManager* tm = Sprite::GetTextureManager();
            if (tm) {
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Texture");
                ImGui::TableSetColumnIndex(1);

                auto names = tm->GetTextureNamesForDebug();
                int currentIndex = 0;
                for (int i = 0; i < (int)names.size(); ++i) {
                    if (names[i] == comp->GetTexturePath()) {
                        currentIndex = i;
                        break;
                    }
                }
                const char* currentPreview = names.empty() ? "" : names[currentIndex].c_str();
                ImGui::PushItemWidth(-1);
                if (ImGui::BeginCombo("##Texture", currentPreview)) {
                    for (int i = 0; i < names.size(); ++i) {
                        bool isSelected = (currentIndex == i);
                        if (ImGui::Selectable(names[i].c_str(), isSelected)) {
                            std::string oldTex = comp->GetTexturePath();
                            std::string newTex = names[i];
                            ComponentUIHelpers::PushInstantUndo(
                                actionManager, oldTex, newTex,
                                std::function<void(const std::string&)>(
                                    [comp](const std::string& v) { comp->SetTexture(v); }));
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
            } else {
                char buffer[256];
                strncpy_s(buffer, sizeof(buffer), comp->GetTexturePath().c_str(), _TRUNCATE);
                static std::string startTex;
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("TexturePath");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##TexturePath", buffer, sizeof(buffer))) {
                    comp->SetTexture(buffer);
                }
                ImGui::PopItemWidth();
                if (ImGui::IsItemActivated()) {
                    startTex = comp->GetTexturePath();
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string endTex = buffer;
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                        startTex, endTex, [comp](const std::string& v) { comp->SetTexture(v); }));
                }
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(EditorDragDrop::PayloadAssetPath)) {
                    std::string droppedPathStr = static_cast<const char*>(payload->Data);
                    std::filesystem::path droppedPath(reinterpret_cast<const char8_t*>(droppedPathStr.c_str()));
                    std::string ext = droppedPath.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga") {
                        std::string newTexName = droppedPathStr;
                        std::replace(newTexName.begin(), newTexName.end(), '\\', '/');
                        std::string lowerPath = newTexName;
                        std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
                        if (lowerPath.find("resources/texture/") == 0) {
                            newTexName = newTexName.substr(18); // length of "resources/texture/"
                        } else if (lowerPath.find("resources/") == 0) {
                            newTexName = newTexName.substr(10);
                        }
                        std::string oldTex = comp->GetTexturePath();
                        ComponentUIHelpers::PushInstantUndo(actionManager, oldTex, newTexName,
                                                            std::function<void(const std::string&)>(
                                                                [comp](const std::string& v) { comp->SetTexture(v); }));
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ComponentUIHelpers::DrawPropertyResetButton(
                "##TexReset", !comp->GetTexturePath().empty() && comp->GetTexturePath() != "whiteTexture.png", [&]() {
                    std::string oldTex = comp->GetTexturePath();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldTex, std::string("whiteTexture.png"),
                        std::function<void(const std::string&)>([comp](const std::string& v) { comp->SetTexture(v); }));
                });

            bool isTopMost = comp->IsTopMost();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("TopMost");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("##TopMost", &isTopMost)) {
                ComponentUIHelpers::PushInstantUndo(actionManager, comp->IsTopMost(), isTopMost,
                                                    std::function<void(const bool&)>([comp](const bool& v) {
                                                        comp->SetTopMost(v);
                                                    }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##TopMostReset", isTopMost, [&]() {
                bool oldTopMost = comp->IsTopMost();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldTopMost, false,
                                                    std::function<void(const bool&)>([comp](const bool& v) {
                                                        comp->SetTopMost(v);
                                                    }));
            });

            bool isFlipX = comp->IsFlipX();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Flip X");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("##Flip X", &isFlipX)) {
                ComponentUIHelpers::PushInstantUndo(actionManager, comp->IsFlipX(), isFlipX,
                                                    std::function<void(const bool&)>([comp](const bool& v) {
                                                        comp->SetFlipX(v);
                                                    }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##FlipXReset", isFlipX, [&]() {
                bool oldFlipX = comp->IsFlipX();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldFlipX, false,
                                                    std::function<void(const bool&)>([comp](const bool& v) {
                                                        comp->SetFlipX(v);
                                                    }));
            });

            bool isFlipY = comp->IsFlipY();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Flip Y");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("##Flip Y", &isFlipY)) {
                ComponentUIHelpers::PushInstantUndo(actionManager, comp->IsFlipY(), isFlipY,
                                                    std::function<void(const bool&)>([comp](const bool& v) {
                                                        comp->SetFlipY(v);
                                                    }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##FlipYReset", isFlipY, [&]() {
                bool oldFlipY = comp->IsFlipY();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldFlipY, false,
                                                    std::function<void(const bool&)>([comp](const bool& v) {
                                                        comp->SetFlipY(v);
                                                    }));
            });

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Anchor");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            Irufemi::Vector2 anchor = comp->GetAnchor();
            if (ImGui::SliderFloat2("##Anchor", &anchor.x, 0.0f, 1.0f)) {
                comp->SetAnchor(anchor);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &anchor,
                std::function<void(const Irufemi::Vector2&)>([comp](const Irufemi::Vector2& v) {
                    comp->SetAnchor(v);
                }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##AnchorReset", comp->GetAnchor().x != 0.5f || comp->GetAnchor().y != 0.5f, [&]() {
                    Irufemi::Vector2 oldA = comp->GetAnchor();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldA, Irufemi::Vector2{0.5f, 0.5f},
                        std::function<void(const Irufemi::Vector2&)>([comp](const Irufemi::Vector2& v) {
                            comp->SetAnchor(v);
                        }));
                });

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Base Size");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            Irufemi::Vector2 baseSize = comp->GetBaseSize();
            if (ImGui::DragFloat2("##Base Size", &baseSize.x, 1.0f, 1.0f, 8192.0f)) {
                comp->SetBaseSize(baseSize);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &baseSize,
                std::function<void(const Irufemi::Vector2&)>([comp](const Irufemi::Vector2& v) { comp->SetBaseSize(v); }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##BaseSizeReset", comp->GetBaseSize().x != 100.0f || comp->GetBaseSize().y != 100.0f, [&]() {
                    Irufemi::Vector2 oldS = comp->GetBaseSize();
                    ComponentUIHelpers::PushInstantUndo(actionManager, oldS, Irufemi::Vector2{100, 100},
                                                        std::function<void(const Irufemi::Vector2&)>([comp](const Irufemi::Vector2& v) { comp->SetBaseSize(v); }));
                });

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Color");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            Irufemi::Vector4 color = comp->GetColor();
            if (ImGui::ColorEdit4("##Color", &color.x)) {
                comp->SetColor(color);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &color,
                std::function<void(const Irufemi::Vector4&)>([comp](const Irufemi::Vector4& v) {
                    comp->SetColor(v);
                }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ColorReset",
                comp->GetColor().x != 1.0f || comp->GetColor().y != 1.0f || comp->GetColor().z != 1.0f || comp->GetColor().w != 1.0f,
                [&]() {
                    Irufemi::Vector4 oldC = comp->GetColor();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldC, Irufemi::Vector4{1, 1, 1, 1},
                        std::function<void(const Irufemi::Vector4&)>([comp](const Irufemi::Vector4& v) {
                            comp->SetColor(v);
                        }));
                });

            ComponentUIHelpers::EndPropertyTable();
        }
        ImGui::TreePop();
    }
}
#endif // EditorMode
