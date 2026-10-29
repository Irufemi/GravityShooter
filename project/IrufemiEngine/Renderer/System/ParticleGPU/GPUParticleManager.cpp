#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include <Windows.h>
#include <cstdio>

void GPUParticleManager::Initialize() {
    auto state = state_.Write();
    state->systems.clear();
    state->idLookup.clear();
    state->nextSystemId = 1;
}

void GPUParticleManager::Update() {
    auto state = state_.Write();
    for (auto& pair : state->systems) {
        pair.second.system->Update();
    }
}

void GPUParticleManager::Draw() {
    auto state = state_.Write();
    for (auto& pair : state->systems) {
        pair.second.system->Draw();
    }
}

void GPUParticleManager::Finalize() {
    auto state = state_.Write();
    state->systems.clear();
    state->idLookup.clear();
    state->nextSystemId = 1;
}

void GPUParticleManager::ClearAllParticles() {
    auto state = state_.Write();
    for (auto& pair : state->systems) {
        pair.second.system->Clear();
    }
}

void GPUParticleManager::WarmUp() {
    auto state = state_.Write();
    for (auto& pair : state->systems) {
        if (pair.second.system) {
            pair.second.system->Clear();
        }
    }
}

GPUParticleManager::EmitterHandle GPUParticleManager::RegisterEmitter(const std::string& texturePath,
                                                                      Irufemi::BlendMode blendMode, bool isUnscaledTime,
                                                                      bool enableLighting,
                                                                      PSOManager::DepthWrite depthWrite) {
    auto state = state_.Write();
    SystemKey key{texturePath, blendMode, isUnscaledTime, enableLighting, depthWrite};
    auto& ctx = state->systems[key];

    if (!ctx.system) {
        // Create new system
        ctx.systemId = state->nextSystemId++;
        ctx.system = std::make_unique<GPUParticleSystem>();
        ctx.system->Initialize(texturePath);
        ctx.system->SetBlendMode(blendMode);
        ctx.system->SetUnscaledTime(isUnscaledTime);
        ctx.system->SetEnableLighting(enableLighting);
        ctx.system->SetDepthWrite(depthWrite);
        ctx.system->SetGlobalFields(&state->globalFields);
        state->idLookup[ctx.systemId] = &ctx;
    }

    uint32_t assignedIndex = 0;
    if (!ctx.freeIndices.empty()) {
        assignedIndex = ctx.freeIndices.back();
        ctx.freeIndices.pop_back();
    } else {
        assignedIndex = ctx.nextIndex++;
        // Resize emittersData_ to accommodate the new index if needed
        if (assignedIndex >= ctx.system->emittersData_.size()) {
            ctx.system->emittersData_.resize(assignedIndex + 1);
        }
    }

    if (assignedIndex >= ctx.slotGenerations.size()) {
        ctx.slotGenerations.resize(assignedIndex + 1, 1);
    }

    // Initialize the slot
    ctx.system->emittersData_[assignedIndex] = GPUParticleEmitter();

    EmitterHandle handle;
    handle.systemId = ctx.systemId;
    handle.emitterIndex = static_cast<uint16_t>(assignedIndex);
    handle.generation = ctx.slotGenerations[assignedIndex];
    return handle;
}

void GPUParticleManager::UnregisterEmitter(const EmitterHandle& handle) {
    if (!handle.IsValid()) {
        return;
    }

    auto state = state_.Write();
    auto it = state->idLookup.find(handle.systemId);
    if (it != state->idLookup.end()) {
        auto* ctx = it->second;
        if (handle.emitterIndex < ctx->slotGenerations.size() &&
            ctx->slotGenerations[handle.emitterIndex] == handle.generation) {
            if (handle.emitterIndex < ctx->system->emittersData_.size()) {
                ctx->system->emittersData_[handle.emitterIndex].emit = 0;
            }
            // 世代番号をインクリメントして古いハンドルを無効化
            ctx->slotGenerations[handle.emitterIndex]++;
            ctx->freeIndices.push_back(handle.emitterIndex);
        }
    }
}

void GPUParticleManager::UpdateEmitterData(const EmitterHandle& handle, const GPUParticleEmitter& data) {
    if (!handle.IsValid()) {
        return;
    }

    auto state = state_.Write();
    auto it = state->idLookup.find(handle.systemId);
    if (it != state->idLookup.end()) {
        auto* ctx = it->second;
        if (handle.emitterIndex < ctx->slotGenerations.size() &&
            ctx->slotGenerations[handle.emitterIndex] == handle.generation &&
            handle.emitterIndex < ctx->system->emittersData_.size()) {
            auto& current = ctx->system->emittersData_[handle.emitterIndex];
            uint32_t burst = current.burstCount + data.burstCount;
            float residue = current.emissionResidue;

            // 前フレーム位置を保持（初回・停止復帰・瞬間移動時は現在位置にリセットして飛び跳ね防止）
            float prevX = current.translateX;
            float prevY = current.translateY;
            float prevZ = current.translateZ;
            float dx = data.translateX - prevX;
            float dy = data.translateY - prevY;
            float dz = data.translateZ - prevZ;
            float distSq = dx * dx + dy * dy + dz * dz;

            // テレポート判定用のしきい値（1フレームで100unit以上離れた場合はワープとみなし補間をリセット）
            constexpr float kMaxTeleportDistanceSq = 10000.0f; // 100.0f * 100.0f

            if (current.emit == 0 || distSq > kMaxTeleportDistanceSq) {
                prevX = data.translateX;
                prevY = data.translateY;
                prevZ = data.translateZ;
            }

            current = data;
            current.burstCount = burst;
            current.emissionResidue = residue;
            current.prevTranslateX = prevX;
            current.prevTranslateY = prevY;
            current.prevTranslateZ = prevZ;
        }
    }
}

void GPUParticleManager::SetMeshEmitterBuffer(EmitterHandle handle, D3D12_GPU_VIRTUAL_ADDRESS vbAddress) {
    if (!handle.IsValid()) {
        return;
    }

    auto state = state_.Write();
    auto it = state->idLookup.find(handle.systemId);
    if (it != state->idLookup.end()) {
        auto* ctx = it->second;
        if (handle.emitterIndex < ctx->slotGenerations.size() &&
            ctx->slotGenerations[handle.emitterIndex] == handle.generation &&
            handle.emitterIndex < ctx->system->emittersData_.size()) {
            if (ctx->system->meshVertexBuffers_.size() <= handle.emitterIndex) {
                ctx->system->meshVertexBuffers_.resize(handle.emitterIndex + 1, 0);
            }
            ctx->system->meshVertexBuffers_[handle.emitterIndex] = vbAddress;
        }
    }
}

GPUParticleManager::FieldHandle GPUParticleManager::RegisterField() {
    auto state = state_.Write();
    uint32_t assignedIndex = 0;
    if (!state->freeFieldIndices.empty()) {
        assignedIndex = state->freeFieldIndices.back();
        state->freeFieldIndices.pop_back();
    } else {
        assignedIndex = state->nextFieldIndex++;
        if (assignedIndex >= state->globalFields.size()) {
            state->globalFields.resize(assignedIndex + 1);
        }
    }

    // Initialize slot with disabled field (strength = 0)
    state->globalFields[assignedIndex] = ParticleField();

    FieldHandle handle;
    handle.index = assignedIndex;
    return handle;
}

void GPUParticleManager::UnregisterField(const FieldHandle& handle) {
    if (!handle.IsValid()) {
        return;
    }

    auto state = state_.Write();
    if (handle.index < state->globalFields.size()) {
        state->globalFields[handle.index].strength = 0.0f; // Disable
        state->freeFieldIndices.push_back(handle.index);
    }
}

void GPUParticleManager::UpdateFieldData(const FieldHandle& handle, const ParticleField& data) {
    if (!handle.IsValid()) {
        return;
    }

    auto state = state_.Write();
    if (handle.index < state->globalFields.size()) {
        state->globalFields[handle.index] = data;
    }
}

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

int GPUParticleManager::GetTotalEmittersUsed() const {
    auto state = state_.Read();
    int totalEmitters = 0;
    for (const auto& pair : state->systems) {
        // nextIndex はこれまで割り当てた最大のインデックス（サイズ）。そこから解放済みの数を引く
        totalEmitters += (int)(pair.second.nextIndex - pair.second.freeIndices.size());
    }
    return totalEmitters;
}

void GPUParticleManager::Debug() {
#if defined(USE_IMGUI)
    if (ImGui::BeginTabItem("GPU Particle Manager")) {
        if (ImGui::BeginChild("GPUParticleScroll", ImVec2(0, 0), false)) {
            auto state = state_.Write();
            ImGui::TextDisabled("(Global system stats moved to TelemetryMonitor)");
            ImGui::Separator();

            ImGui::Spacing();
            ImGui::Text("System Details per Texture");
            for (auto& pair : state->systems) {
                const std::string& textureName = pair.first.texturePath;
                auto& context = pair.second;

                if (ImGui::TreeNode(textureName.c_str())) {
                    ImGui::Text("Emitters: %d / %d", (int)(context.nextIndex - context.freeIndices.size()),
                                GPUParticleSystem::kMaxEmitters);
                    context.system->Debug();
                    ImGui::TreePop();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndTabItem();
    }
#endif
}
