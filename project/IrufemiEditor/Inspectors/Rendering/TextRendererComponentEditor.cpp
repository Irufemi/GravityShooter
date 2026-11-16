#include "Inspectors/Rendering/TextRendererComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "UI/ComponentUIHelpers.h"
#include "Framework/Component/Renderer/TextRendererComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "Renderer/Font/FontManager.h"
#include "Core/Utility/StringUtility.h"

using namespace Irufemi;

void TextRendererComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* comp = static_cast<TextRendererComponent*>(component);
    bool headerOpen = ImGui::TreeNodeEx("TextRenderer", ImGuiTreeNodeFlags_DefaultOpen);

    ComponentUIHelpers::DrawComponentContextMenu(comp, actionManager);

    if (headerOpen) {
        if (ComponentUIHelpers::BeginPropertyTable("TextRendererTable")) {
            // Text (UTF-8)
            std::string utf8Text = ConvertString(comp->GetText());
            char textBuffer[256];
            strncpy_s(textBuffer, sizeof(textBuffer), utf8Text.c_str(), _TRUNCATE);
            static std::string startText;
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Text");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##Text", textBuffer, sizeof(textBuffer))) {
                comp->SetText(ConvertString(std::string(textBuffer)));
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemActivated()) {
                startText = utf8Text;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                std::string endText = textBuffer;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                    startText, endText, [comp](const std::string& v) { comp->SetText(ConvertString(v)); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##TextReset", !utf8Text.empty(), [&]() {
                std::string oldT = utf8Text;
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldT, std::string(""),
                    std::function<void(const std::string&)>(
                        [comp](const std::string& v) { comp->SetText(ConvertString(v)); }));
            });

            // Font ID
            std::string fontId = comp->GetFontId();
            FontManager* fm = Text::GetFontManager();
            if (fm) {
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Font ID");
                ImGui::TableSetColumnIndex(1);
                auto fontIds = fm->GetLoadedFontIds();
                int currentIndex = 0;
                for (int i = 0; i < (int)fontIds.size(); ++i) {
                    if (fontIds[i] == fontId) {
                        currentIndex = i;
                        break;
                    }
                }
                const char* currentPreview = fontIds.empty() ? "" : fontIds[currentIndex].c_str();
                ImGui::PushItemWidth(-1);
                if (ImGui::BeginCombo("##Font ID", currentPreview)) {
                    for (int i = 0; i < fontIds.size(); ++i) {
                        bool isSelected = (currentIndex == i);
                        if (ImGui::Selectable(fontIds[i].c_str(), isSelected)) {
                            std::string oldFontId = comp->GetFontId();
                            std::string newFontId = fontIds[i];
                            ComponentUIHelpers::PushInstantUndo(
                                actionManager, oldFontId, newFontId,
                                std::function<void(const std::string&)>(
                                    [comp](const std::string& v) { comp->SetFontId(v); }));
                        }
                        if (isSelected) {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();
                ComponentUIHelpers::DrawPropertyResetButton(
                    "##FontReset", !fontId.empty() && fontId != "default", [&]() {
                        std::string oldF = comp->GetFontId();
                        ComponentUIHelpers::PushInstantUndo(actionManager, oldF, std::string("default"),
                                                            std::function<void(const std::string&)>(
                                                                [comp](const std::string& v) { comp->SetFontId(v); }));
                    });
            } else {
                char fontBuffer[128];
                strncpy_s(fontBuffer, sizeof(fontBuffer), fontId.c_str(), _TRUNCATE);
                static std::string startFont;
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Font ID");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::InputText("##Font ID", fontBuffer, sizeof(fontBuffer))) {
                    comp->SetFontId(fontBuffer);
                }
                ImGui::PopItemWidth();
                if (ImGui::IsItemActivated()) {
                    startFont = fontId;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    std::string endFont = fontBuffer;
                    actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                        startFont, endFont, [comp](const std::string& v) { comp->SetFontId(v); }));
                }
                ComponentUIHelpers::DrawPropertyResetButton(
                    "##FontReset2", !fontId.empty() && fontId != "default", [&]() {
                        std::string oldF = comp->GetFontId();
                        ComponentUIHelpers::PushInstantUndo(actionManager, oldF, std::string("default"),
                                                            std::function<void(const std::string&)>(
                                                                [comp](const std::string& v) { comp->SetFontId(v); }));
                    });
            }

            // Alignment
            TextAlignment align = comp->GetAlignment();
            const char* alignments[] = {"Left", "Center", "Right"};
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Alignment");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##Alignment", alignments[static_cast<int>(align)])) {
                for (int i = 0; i < 3; ++i) {
                    bool isSelected = (static_cast<int>(align) == i);
                    if (ImGui::Selectable(alignments[i], isSelected)) {
                        TextAlignment oldAlign = comp->GetAlignment();
                        TextAlignment newAlign = static_cast<TextAlignment>(i);
                        ComponentUIHelpers::PushInstantUndo(
                            actionManager, oldAlign, newAlign,
                            std::function<void(const TextAlignment&)>(
                                [comp](const TextAlignment& v) { comp->SetAlignment(v); }));
                    }
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::DrawPropertyResetButton("##AlignReset", align != TextAlignment::Left, [&]() {
                TextAlignment oldA = comp->GetAlignment();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldA, TextAlignment::Left,
                                                    std::function<void(const TextAlignment&)>(
                                                        [comp](const TextAlignment& v) { comp->SetAlignment(v); }));
            });

            // Base Scale
            float scale = comp->GetBaseScale();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Base Scale");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Base Scale", &scale, 0.1f, 0.1f, 1000.0f)) {
                comp->SetBaseScale(scale);
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &scale,
                std::function<void(const float&)>([comp](const float& v) { comp->SetBaseScale(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##ScaleReset", scale != 100.0f, [&]() {
                float oldS = comp->GetBaseScale();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldS, 100.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetBaseScale(v); }));
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

            // TopMost
            bool isTopMost = comp->IsTopMost();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("TopMost");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("##TopMost", &isTopMost)) {
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, comp->IsTopMost(), isTopMost,
                    std::function<void(const bool&)>([comp](const bool& v) { comp->SetTopMost(v); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##TopMostReset", isTopMost, [&]() {
                bool oldTopMost = comp->IsTopMost();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldTopMost, false,
                    std::function<void(const bool&)>([comp](const bool& v) { comp->SetTopMost(v); }));
            });

            ComponentUIHelpers::EndPropertyTable();
        }

        ImGui::TreePop();
    }
}
#endif // EditorMode
