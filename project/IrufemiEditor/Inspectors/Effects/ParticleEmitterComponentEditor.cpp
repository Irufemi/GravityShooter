#include "Inspectors/Effects/ParticleEmitterComponentEditor.h"

#ifdef EditorMode
#include <imgui/imgui.h>
#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Renderer/Object/Particle/ParticleObject.h"
#include "UI/ComponentUIHelpers.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"
#include "Core/System/IrufemiEngine.h"
#include "Resource/Texture/TextureManager.h"

void ParticleEmitterComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto* compWrapper = static_cast<ParticleEmitterComponent*>(component);
    auto* comp = compWrapper->GetParticleObject();

    ImGui::PushID(compWrapper);
    bool headerOpen = ImGui::CollapsingHeader("Particle Emitter", ImGuiTreeNodeFlags_DefaultOpen);

    if (ComponentUIHelpers::DrawComponentContextMenu(component, actionManager)) {
        ImGui::PopID();
        return;
    }

    if (headerOpen) {
        if (ComponentUIHelpers::BeginPropertyTable("ParticleGeneral")) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "General");
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(2);
            ImGui::Separator();

            auto* textureManager = ParticleObject::GetTextureManager();
            if (textureManager && !textureManager->GetTextureNamesForDebug().empty()) {
                auto textureNames = textureManager->GetTextureNamesForDebug();
                std::vector<const char*> namesCStr;
                int currentIndex = -1;
                for (int i = 0; i < textureNames.size(); ++i) {
                    namesCStr.push_back(textureNames[i].c_str());
                    if (comp->GetTexturePath() == textureNames[i]) {
                        currentIndex = i;
                    }
                }
                if (currentIndex == -1) {
                    currentIndex = 0;
                }

                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Texture");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::Combo("##Texture", &currentIndex, namesCStr.data(), (int)namesCStr.size())) {
                    std::string oldPath = comp->GetTexturePath();
                    std::string newPath = textureNames[currentIndex];
                    ComponentUIHelpers::PushInstantUndo(actionManager, oldPath, newPath,
                                                        std::function<void(const std::string&)>(
                                                            [comp](const std::string& v) { comp->SetTexturePath(v); }));
                }
                ImGui::PopItemWidth();
                ComponentUIHelpers::DrawPropertyResetButton(
                    "##TexReset", !comp->GetTexturePath().empty() && comp->GetTexturePath() != "resources/circle.png",
                    [&]() {
                        std::string oldPath = comp->GetTexturePath();
                        ComponentUIHelpers::PushInstantUndo(
                            actionManager, oldPath, std::string("resources/circle.png"),
                            std::function<void(const std::string&)>(
                                [comp](const std::string& v) { comp->SetTexturePath(v); }));
                    });
            }

            const char* blendNames[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Premultiplied"};
            int currentBlend = static_cast<int>(comp->GetBlendMode());
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Blend Mode");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##Blend Mode", &currentBlend, blendNames, 7)) {
                auto oldB = comp->GetBlendMode();
                auto newB = static_cast<Irufemi::BlendMode>(currentBlend);
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldB, newB,
                    std::function<void(const Irufemi::BlendMode&)>(
                        [comp](const Irufemi::BlendMode& v) { comp->SetBlendMode(v); }));
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::DrawPropertyResetButton("##BlendReset", currentBlend != 2, [&]() {
                auto oldB = comp->GetBlendMode();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldB, Irufemi::BlendMode::kBlendModeAdd,
                    std::function<void(const Irufemi::BlendMode&)>(
                        [comp](const Irufemi::BlendMode& v) { comp->SetBlendMode(v); }));
            });

            bool emitAwake = comp->GetEmitOnAwake();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Emit On Awake");
            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("##EmitAwake", &emitAwake)) {
                bool oldE = comp->GetEmitOnAwake();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldE, emitAwake,
                    std::function<void(const bool&)>([comp](const bool& v) { comp->SetEmitOnAwake(v); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##AwakeReset", !comp->GetEmitOnAwake(), [&]() {
                bool oldE = comp->GetEmitOnAwake();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldE, true,
                    std::function<void(const bool&)>([comp](const bool& v) { comp->SetEmitOnAwake(v); }));
            });

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Shape & Emission");
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(2);
            ImGui::Separator();

            const char* shapeNames[] = {"Sphere", "Beam", "Box", "Cylinder"};
            int emitType = comp->GetEmitType();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Emit Shape");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##EmitShape", &emitType, shapeNames, 4)) {
                int oldT = comp->GetEmitType();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldT, emitType,
                    std::function<void(const int&)>([comp](const int& v) { comp->SetEmitType(v); }));
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::DrawPropertyResetButton("##ShapeReset", emitType != 0, [&]() {
                int oldT = comp->GetEmitType();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldT, 0,
                    std::function<void(const int&)>([comp](const int& v) { comp->SetEmitType(v); }));
            });

            if (emitType == 2) {
                Irufemi::Vector3 aSize = comp->GetAreaSize();
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Area Size");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::DragFloat3("##AreaSize", &aSize.x, 0.1f, 0.0f, 100.0f)) {
                }
                ImGui::PopItemWidth();
                ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &aSize,
                                                      std::function<void(const Irufemi::Vector3&)>(
                                                          [comp](const Irufemi::Vector3& v) { comp->SetAreaSize(v); }));
                ComponentUIHelpers::DrawPropertyResetButton(
                    "##AreaReset", aSize.x != 10.0f || aSize.y != 10.0f || aSize.z != 10.0f, [&]() {
                        Irufemi::Vector3 oldA = comp->GetAreaSize();
                        ComponentUIHelpers::PushInstantUndo(
                            actionManager, oldA, Irufemi::Vector3{10, 10, 10},
                            std::function<void(const Irufemi::Vector3&)>(
                                [comp](const Irufemi::Vector3& v) { comp->SetAreaSize(v); }));
                    });
            } else {
                float rad = comp->GetRadius();
                ImGui::TableNextRow();
                ComponentUIHelpers::DrawPropertyLabel("Radius");
                ImGui::TableSetColumnIndex(1);
                ImGui::PushItemWidth(-1);
                if (ImGui::DragFloat("##Radius", &rad, 0.1f, 0.0f, 100.0f)) {
                }
                ImGui::PopItemWidth();
                ComponentUIHelpers::CheckUndoRedoDrag(
                    actionManager, &rad,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetRadius(v); }));
                ComponentUIHelpers::DrawPropertyResetButton("##RadReset", rad != 0.0f, [&]() {
                    float oldR = comp->GetRadius();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldR, 0.0f,
                        std::function<void(const float&)>([comp](const float& v) { comp->SetRadius(v); }));
                });
            }

            float spread = comp->GetSpread();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Spread");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Spread", &spread, 0.01f, 0.0f, 1.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &spread,
                std::function<void(const float&)>([comp](const float& v) { comp->SetSpread(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##SpreadReset", spread != 0.1f, [&]() {
                float oldS = comp->GetSpread();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldS, 0.1f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetSpread(v); }));
            });

            float eRate = comp->GetEmissionRate();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Emission Rate");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##EmissionRate", &eRate, 1.0f, 0.0f, 10000.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &eRate,
                std::function<void(const float&)>([comp](const float& v) { comp->SetEmissionRate(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##ERateReset", eRate != 50.0f, [&]() {
                float oldE = comp->GetEmissionRate();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldE, 50.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetEmissionRate(v); }));
            });

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Physics");
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(2);
            ImGui::Separator();

            float vel = comp->GetVelocity();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Velocity");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Velocity", &vel, 0.1f, 0.0f, 100.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &vel,
                std::function<void(const float&)>([comp](const float& v) { comp->SetVelocity(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##VelReset", vel != 1.0f, [&]() {
                float oldV = comp->GetVelocity();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldV, 1.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetVelocity(v); }));
            });

            float grav = comp->GetGravity();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Gravity");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Gravity", &grav, 0.1f, -50.0f, 50.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &grav,
                std::function<void(const float&)>([comp](const float& v) { comp->SetGravity(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##GravReset", grav != 0.0f, [&]() {
                float oldV = comp->GetGravity();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldV, 0.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetGravity(v); }));
            });

            float jitter = comp->GetJitter();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Jitter");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Jitter", &jitter, 0.01f, 0.0f, 10.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &jitter,
                std::function<void(const float&)>([comp](const float& v) { comp->SetJitter(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##JitReset", jitter != 0.0f, [&]() {
                float oldV = comp->GetJitter();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldV, 0.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetJitter(v); }));
            });

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Visuals");
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(2);
            ImGui::Separator();

            float lifeMin = comp->GetLifeTimeMin();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Life Min");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##LifeMin", &lifeMin, 0.05f, 0.01f, 10.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &lifeMin,
                std::function<void(const float&)>([comp](const float& v) { comp->SetLifeTimeMin(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##LMinReset", lifeMin != 0.5f, [&]() {
                float oldV = comp->GetLifeTimeMin();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldV, 0.5f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetLifeTimeMin(v); }));
            });

            float lifeMax = comp->GetLifeTimeMax();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Life Max");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##LifeMax", &lifeMax, 0.05f, 0.01f, 10.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &lifeMax,
                std::function<void(const float&)>([comp](const float& v) { comp->SetLifeTimeMax(v); }));
            ComponentUIHelpers::DrawPropertyResetButton("##LMaxReset", lifeMax != 1.0f, [&]() {
                float oldV = comp->GetLifeTimeMax();
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldV, 1.0f,
                    std::function<void(const float&)>([comp](const float& v) { comp->SetLifeTimeMax(v); }));
            });

            Irufemi::Vector4 col = comp->GetColor();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Start Color");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::ColorEdit4("##Color", &col.x)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(
                actionManager, &col,
                std::function<void(const Irufemi::Vector4&)>([comp](const Irufemi::Vector4& v) { comp->SetColor(v); }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ColReset", col.x != 1.0f || col.y != 1.0f || col.z != 1.0f, [&]() {
                    Irufemi::Vector4 oldC = comp->GetColor();
                    ComponentUIHelpers::PushInstantUndo(actionManager, oldC, Irufemi::Vector4{1, 1, 1, 1},
                                                        std::function<void(const Irufemi::Vector4&)>(
                                                            [comp](const Irufemi::Vector4& v) { comp->SetColor(v); }));
                });

            Irufemi::Vector3 sScale = comp->GetStartScale();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Start Scale");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##SScale", &sScale.x, 0.05f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &sScale,
                                                  std::function<void(const Irufemi::Vector3&)>(
                                                      [comp](const Irufemi::Vector3& v) { comp->SetStartScale(v); }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##SSReset", sScale.x != 1.0f || sScale.y != 1.0f || sScale.z != 1.0f, [&]() {
                    Irufemi::Vector3 oldS = comp->GetStartScale();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldS, Irufemi::Vector3{1, 1, 1},
                        std::function<void(const Irufemi::Vector3&)>(
                            [comp](const Irufemi::Vector3& v) { comp->SetStartScale(v); }));
                });

            Irufemi::Vector3 eScale = comp->GetEndScale();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("End Scale");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat3("##EScale", &eScale.x, 0.05f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &eScale,
                                                  std::function<void(const Irufemi::Vector3&)>(
                                                      [comp](const Irufemi::Vector3& v) { comp->SetEndScale(v); }));
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ESReset", eScale.x != 0.0f || eScale.y != 0.0f || eScale.z != 0.0f, [&]() {
                    Irufemi::Vector3 oldS = comp->GetEndScale();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldS, Irufemi::Vector3{0, 0, 0},
                        std::function<void(const Irufemi::Vector3&)>(
                            [comp](const Irufemi::Vector3& v) { comp->SetEndScale(v); }));
                });

            ComponentUIHelpers::EndPropertyTable();
        }

        if (ImGui::Button("Test Burst (50)")) {
            comp->EmitBurst(50);
        }
        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            comp->Play();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            comp->Stop();
        }
    }
    ImGui::PopID();
}
#endif // EditorMode
