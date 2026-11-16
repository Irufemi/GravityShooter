#include "Inspectors/Rendering/Primitive2DRendererComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include <filesystem>
#include <algorithm>
#include "UI/ComponentUIHelpers.h"
#include "Framework/Component/Renderer/Primitive2DRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "UI/EditorDragDrop.h"
#include "Resource/Texture/TextureManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h" // TextureManager取得用（共通）

using namespace Irufemi;

void Primitive2DRendererComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<Primitive2DRendererComponent*>(component);
    bool headerOpen = ImGui::TreeNodeEx("Primitive2DRenderer", ImGuiTreeNodeFlags_DefaultOpen);

    ComponentUIHelpers::DrawComponentContextMenu(comp, actionManager);

    if (headerOpen) {
        if (ComponentUIHelpers::BeginPropertyTable("Primitive2DTable")) {
            const char* typeNames[] = {"Rect", "Triangle", "Circle", "Ring", "Line"};

            int typeIndex = static_cast<int>(comp->GetShape());
            int oldTypeIndex = typeIndex;
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Shape Type");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##Shape Type", &typeIndex, typeNames, IM_ARRAYSIZE(typeNames))) {
                ComponentUIHelpers::PushInstantUndo(actionManager, oldTypeIndex, typeIndex,
                                                    std::function<void(const int&)>([comp](const int& v) {
                                                        comp->SetShape(static_cast<Irufemi::Primitive2DType>(v));
                                                    }));
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::DrawPropertyResetButton("##ShapeReset", typeIndex != 0, [&]() {
                ComponentUIHelpers::PushInstantUndo(actionManager, oldTypeIndex, 0,
                                                    std::function<void(const int&)>([comp](const int& v) {
                                                        comp->SetShape(static_cast<Irufemi::Primitive2DType>(v));
                                                    }));
            });

            Irufemi::Primitive2DType type = static_cast<Irufemi::Primitive2DType>(typeIndex);

            // Size
            Irufemi::Vector2 size = comp->GetSize();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Size");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat2("##Size", &size.x, 1.0f, 0.0f, 10000.0f)) {
                comp->SetSize(size);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &size,
                std::function<void(const Irufemi::Vector2&)>([comp](const Irufemi::Vector2& v) { comp->SetSize(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##SizeReset", size.x != 100.0f || size.y != 100.0f, [&]() {
                Irufemi::Vector2 oldS = comp->GetSize();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldS, Irufemi::Vector2{100, 100},
                                                    std::function<void(const Irufemi::Vector2&)>(
                                                        [comp](const Irufemi::Vector2& v) { comp->SetSize(v); }));
            });

            // Pivot
            Irufemi::Vector2 pivot = comp->GetPivot();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Pivot");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat2("##Pivot", &pivot.x, 0.01f, 0.0f, 1.0f)) {
                comp->SetPivot(pivot);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &pivot,
                std::function<void(const Irufemi::Vector2&)>([comp](const Irufemi::Vector2& v) { comp->SetPivot(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##PivotReset", pivot.x != 0.5f || pivot.y != 0.5f, [&]() {
                Irufemi::Vector2 oldP = comp->GetPivot();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldP, Irufemi::Vector2{0.5f, 0.5f},
                                                    std::function<void(const Irufemi::Vector2&)>(
                                                        [comp](const Irufemi::Vector2& v) { comp->SetPivot(v); }));
            });

            switch (type) {
            case Irufemi::Primitive2DType::Circle:
            case Irufemi::Primitive2DType::Ring: {
                int sub = comp->GetSubdivision();
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Subdivision");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::SliderInt("##Subdivision", &sub, 3, 128)) {
                    comp->SetSubdivision(sub);
                }
                ImGui::PopItemWidth();
                ComponentUIHelpers::CheckUndoRedoDrag(
                    actionManager, &sub,
                    std::function<void(const int&)>([comp](const int& v) { comp->SetSubdivision(v); }));
                ComponentUIHelpers::DrawPropertyResetButton("##SubDivReset", sub != 32, [&]() {
                    int oldSub = comp->GetSubdivision();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldSub, 32,
                        std::function<void(const int&)>([comp](const int& v) { comp->SetSubdivision(v); }));
                });
            } break;
            default:
                break;
            }

            switch (type) {
            case Irufemi::Primitive2DType::Ring:
            case Irufemi::Primitive2DType::Line: {
                float thick = comp->GetThickness();
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Thickness");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::DragFloat("##Thickness", &thick, 0.1f, 0.1f, 100.0f)) {
                    comp->SetThickness(thick);
                }
                ImGui::PopItemWidth();
                ComponentUIHelpers::CheckUndoRedoDrag(
                    actionManager, &thick,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetThickness(v); }));
                ComponentUIHelpers::DrawPropertyResetButton("##ThicknessReset", thick != 1.0f, [&]() {
                    float oldT = comp->GetThickness();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldT, 1.0f,
                        std::function<void(const float&)>([comp](const float& v) { comp->SetThickness(v); }));
                });
            } break;
            default:
                break;
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Appearance");
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(2);
            ImGui::Separator();

            // TopMost
            bool topMost = comp->IsTopMost();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("TopMost");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("##TopMost", &topMost)) {
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, comp->IsTopMost(), topMost,
                    std::function<void(const bool&)>([comp](const bool& v) { comp->SetTopMost(v); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##TopMostReset", topMost, [&]() {
                bool oldTopMost = comp->IsTopMost();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldTopMost, false,
                    std::function<void(const bool&)>([comp](const bool& v) { comp->SetTopMost(v); }));
            });

            // Color
            Irufemi::Vector4 color = comp->GetColor();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Color");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::ColorEdit4("##Color", &color.x)) {
                comp->SetColor(color);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &color,
                std::function<void(const Irufemi::Vector4&)>([comp](const Irufemi::Vector4& v) { comp->SetColor(v); }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ColorReset", color.x != 1.0f || color.y != 1.0f || color.z != 1.0f || color.w != 1.0f, [&]() {
                    Irufemi::Vector4 oldC = comp->GetColor();
                    ComponentUIHelpers::PushInstantUndo(actionManager, oldC, Irufemi::Vector4{1, 1, 1, 1},
                                                        std::function<void(const Irufemi::Vector4&)>(
                                                            [comp](const Irufemi::Vector4& v) { comp->SetColor(v); }));
                });

            // Texture
            TextureManager* tm = Primitive3DObject::GetTextureManager(); // TextureManagerを共通で取得
            if (tm) {
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Texture");
                ImGui::TableSetColumnIndex(1);

                auto names = tm->GetTextureNamesForDebug();
                int currentIndex = 0;
                // Handle valid ResourceHandle to get the name, but our component stores texturePath directly
                std::string currentTex = comp->GetTexture();
                for (int i = 0; i < (int)names.size(); ++i) {
                    if (names[i] == currentTex) {
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
                            std::string oldTex = currentTex;
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
                                newTexName = newTexName.substr(18);
                            } else if (lowerPath.find("resources/") == 0) {
                                newTexName = newTexName.substr(10);
                            }

                            std::string oldTex = comp->GetTexture();
                            ComponentUIHelpers::PushInstantUndo(
                                actionManager, oldTex, newTexName,
                                std::function<void(const std::string&)>(
                                    [comp](const std::string& v) { comp->SetTexture(v); }));
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ComponentUIHelpers::DrawPropertyResetButton(
                    "##TexReset", !currentTex.empty() && currentTex != "whiteTexture.png", [&]() {
                        std::string oldTex = comp->GetTexture();
                        ComponentUIHelpers::PushInstantUndo(actionManager, oldTex, std::string("whiteTexture.png"),
                                                            std::function<void(const std::string&)>(
                                                                [comp](const std::string& v) { comp->SetTexture(v); }));
                    });
            }

            ComponentUIHelpers::EndPropertyTable();
        }

        ImGui::TreePop();
    }
}
#endif // EditorMode
