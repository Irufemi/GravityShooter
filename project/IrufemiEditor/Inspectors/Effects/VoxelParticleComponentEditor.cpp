#include "Inspectors/Effects/VoxelParticleComponentEditor.h"
#ifdef EditorMode
#include <imgui.h>
#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include <string>
#include <cstring>

#include "UI/ComponentUIHelpers.h"
#include "Commands/EditorActionManager.h"
#include "Commands/EditorCommands.h"

using namespace Irufemi;

void VoxelParticleComponentEditor::Draw(Component* component, EditorActionManager* actionManager) {
    auto voxelComponent = dynamic_cast<VoxelParticleComponent*>(component);
    if (!voxelComponent) {
        return;
    }

    if (ImGui::CollapsingHeader("VoxelParticle Component", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ComponentUIHelpers::BeginPropertyTable("VoxelParticleTable")) {
            char buf[256];
            strncpy_s(buf, sizeof(buf), voxelComponent->GetOverrideModelName().c_str(), _TRUNCATE);
            static std::string startStr;
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Override Model");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputText("##Override Model", buf, sizeof(buf))) {
                voxelComponent->SetOverrideModelName(buf);
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemActivated()) {
                startStr = voxelComponent->GetOverrideModelName();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                std::string endStr = buf;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<std::string>>(
                    startStr, endStr,
                    [voxelComponent](const std::string& v) { voxelComponent->SetOverrideModelName(v); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ModelReset", !voxelComponent->GetOverrideModelName().empty(), [&]() {
                    std::string oldM = voxelComponent->GetOverrideModelName();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldM, std::string(""),
                        std::function<void(const std::string&)>(
                            [voxelComponent](const std::string& v) { voxelComponent->SetOverrideModelName(v); }));
                });

            Irufemi::Vector3Int resolution = voxelComponent->GetResolution();
            int res[3] = {resolution.x, resolution.y, resolution.z};
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Resolution");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputInt3("##Resolution", res)) {
                voxelComponent->SetResolution({res[0], res[1], res[2]});
            }
            ImGui::PopItemWidth();
            static Irufemi::Vector3Int startRes;
            if (ImGui::IsItemActivated()) {
                startRes = resolution;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                Irufemi::Vector3Int endRes = {res[0], res[1], res[2]};
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<Irufemi::Vector3Int>>(
                    startRes, endRes,
                    [voxelComponent](const Irufemi::Vector3Int& v) { voxelComponent->SetResolution(v); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton(
                "##ResReset", resolution.x != 10 || resolution.y != 10 || resolution.z != 10, [&]() {
                    Irufemi::Vector3Int oldR = voxelComponent->GetResolution();
                    ComponentUIHelpers::PushInstantUndo(
                        actionManager, oldR, Irufemi::Vector3Int{10, 10, 10},
                        std::function<void(const Irufemi::Vector3Int&)>(
                            [voxelComponent](const Irufemi::Vector3Int& v) { voxelComponent->SetResolution(v); }));
                });

            int preAllocate = voxelComponent->GetPreAllocateCount();
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("PreAllocate Count");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::InputInt("##PreAllocate Count", &preAllocate)) {
                voxelComponent->SetPreAllocateCount(preAllocate);
            }
            ImGui::PopItemWidth();
            static int startPre;
            if (ImGui::IsItemActivated()) {
                startPre = voxelComponent->GetPreAllocateCount();
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                int endPre = preAllocate;
                actionManager->PushAndExecute(std::make_unique<ChangeValueCommand<int>>(
                    startPre, endPre, [voxelComponent](const int& v) { voxelComponent->SetPreAllocateCount(v); }));
            }
            ComponentUIHelpers::DrawPropertyResetButton("##PreAllocReset", preAllocate != 1000, [&]() {
                int oldP = voxelComponent->GetPreAllocateCount();
                ComponentUIHelpers::PushInstantUndo(actionManager, oldP, 1000,
                                                    std::function<void(const int&)>([voxelComponent](const int& v) {
                                                        voxelComponent->SetPreAllocateCount(v);
                                                    }));
            });

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Emitter Params");
            ImGui::TableSetColumnIndex(1);
            ImGui::Separator();
            ImGui::TableSetColumnIndex(2);
            ImGui::Separator();

            auto& emitterParams = voxelComponent->GetEmitterParams();

            const char* particleTypes[] = {"Default",     "Building",           "AshDisintegration",
                                           "FineScatter", "DebrisLargeGravity", "DebrisExplosive"};
            int currentType = static_cast<int>(emitterParams.particleType);
            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Particle Type");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::Combo("##Particle Type", &currentType, particleTypes, IM_ARRAYSIZE(particleTypes))) {
                auto oldType = emitterParams.particleType;
                auto newType = static_cast<uint32_t>(currentType);
                ComponentUIHelpers::PushInstantUndo(
                    actionManager, oldType, newType,
                    std::function<void(const uint32_t&)>(
                        [voxelComponent](const uint32_t& v) { voxelComponent->GetEmitterParams().particleType = v; }));
            }
            ImGui::PopItemWidth();

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("LifeTime");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##LifeTime", &emitterParams.lifeTime, 0.1f, 0.1f, 10.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &emitterParams.lifeTime,
                                                  std::function<void(const float&)>([voxelComponent](const float& v) {
                                                      voxelComponent->GetEmitterParams().lifeTime = v;
                                                  }));
            ComponentUIHelpers::DrawPropertyResetButton("##LifeReset", emitterParams.lifeTime != 2.0f, [&]() {
                float oldV = voxelComponent->GetEmitterParams().lifeTime;
                ComponentUIHelpers::PushInstantUndo(actionManager, oldV, 2.0f,
                                                    std::function<void(const float&)>([voxelComponent](const float& v) {
                                                        voxelComponent->GetEmitterParams().lifeTime = v;
                                                    }));
            });

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Gravity");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Gravity", &emitterParams.gravity, 0.1f, -20.0f, 100.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &emitterParams.gravity,
                                                  std::function<void(const float&)>([voxelComponent](const float& v) {
                                                      voxelComponent->GetEmitterParams().gravity = v;
                                                  }));
            ComponentUIHelpers::DrawPropertyResetButton("##GravReset", emitterParams.gravity != 9.8f, [&]() {
                float oldV = voxelComponent->GetEmitterParams().gravity;
                ComponentUIHelpers::PushInstantUndo(actionManager, oldV, 9.8f,
                                                    std::function<void(const float&)>([voxelComponent](const float& v) {
                                                        voxelComponent->GetEmitterParams().gravity = v;
                                                    }));
            });

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Dispersion");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Dispersion", &emitterParams.dispersion, 0.1f, 0.0f, 100.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &emitterParams.dispersion,
                                                  std::function<void(const float&)>([voxelComponent](const float& v) {
                                                      voxelComponent->GetEmitterParams().dispersion = v;
                                                  }));
            ComponentUIHelpers::DrawPropertyResetButton("##DispReset", emitterParams.dispersion != 1.0f, [&]() {
                float oldV = voxelComponent->GetEmitterParams().dispersion;
                ComponentUIHelpers::PushInstantUndo(actionManager, oldV, 1.0f,
                                                    std::function<void(const float&)>([voxelComponent](const float& v) {
                                                        voxelComponent->GetEmitterParams().dispersion = v;
                                                    }));
            });

            ImGui::TableNextRow();
            ComponentUIHelpers::DrawPropertyLabel("Convergence");
            ImGui::TableSetColumnIndex(1);
            ImGui::PushItemWidth(-1);
            if (ImGui::DragFloat("##Convergence", &emitterParams.convergence, 0.01f, 0.0f, 1.0f)) {
            }
            ImGui::PopItemWidth();
            ComponentUIHelpers::CheckUndoRedoDrag(actionManager, &emitterParams.convergence,
                                                  std::function<void(const float&)>([voxelComponent](const float& v) {
                                                      voxelComponent->GetEmitterParams().convergence = v;
                                                  }));
            ComponentUIHelpers::DrawPropertyResetButton("##ConvReset", emitterParams.convergence != 0.0f, [&]() {
                float oldV = voxelComponent->GetEmitterParams().convergence;
                ComponentUIHelpers::PushInstantUndo(actionManager, oldV, 0.0f,
                                                    std::function<void(const float&)>([voxelComponent](const float& v) {
                                                        voxelComponent->GetEmitterParams().convergence = v;
                                                    }));
            });

            ComponentUIHelpers::EndPropertyTable();
        }

        if (ImGui::Button("Test Explode")) {
            voxelComponent->Explode();
        }
    }
}
#endif // EditorMode
