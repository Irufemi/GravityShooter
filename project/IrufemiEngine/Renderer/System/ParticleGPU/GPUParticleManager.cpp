#include "Renderer/System/ParticleGPU/GPUParticleManager.h"
#include "Renderer/DrawManager.h"
#include "Core/System/IrufemiEngine.h"
#include <Windows.h>
#include <cstdio>

void GPUParticleManager::Initialize() {
    systems_.clear();
    idLookup_.clear();
    nextSystemId_ = 1;
}

void GPUParticleManager::Update() {
    for (auto& pair : systems_) {
        pair.second.system->Update();
    }
}

void GPUParticleManager::Draw() {
    for (auto& pair : systems_) {
        pair.second.system->Draw();
    }
}

void GPUParticleManager::Finalize() {
    systems_.clear();
    idLookup_.clear();
    nextSystemId_ = 1;
}

void GPUParticleManager::ClearAllParticles() {
    for (auto& pair : systems_) {
        pair.second.system->Clear();
    }
}

void GPUParticleManager::WarmUp() {
    for (auto& pair : systems_) {
        if (pair.second.system) {
            pair.second.system->Clear();
        }
    }
}

GPUParticleManager::EmitterHandle GPUParticleManager::RegisterEmitter(const std::string& texturePath,
                                                                      Irufemi::BlendMode blendMode, bool isUnscaledTime,
                                                                      bool enableLighting,
                                                                      PSOManager::DepthWrite depthWrite) {
    SystemKey key{texturePath, blendMode, isUnscaledTime, enableLighting, depthWrite};
    auto& ctx = systems_[key];

    if (!ctx.system) {
        // Create new system
        ctx.systemId = nextSystemId_++;
        ctx.system = std::make_unique<GPUParticleSystem>();
        ctx.system->Initialize(texturePath);
        ctx.system->SetBlendMode(blendMode);
        ctx.system->SetUnscaledTime(isUnscaledTime);
        ctx.system->SetEnableLighting(enableLighting);
        ctx.system->SetDepthWrite(depthWrite);
        ctx.system->SetGlobalFields(&globalFields_);
        idLookup_[ctx.systemId] = &ctx;
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

    auto it = idLookup_.find(handle.systemId);
    if (it != idLookup_.end()) {
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

    auto it = idLookup_.find(handle.systemId);
    if (it != idLookup_.end()) {
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

            if (current.emit == 0 || distSq > 10000.0f || (prevX == 0.0f && prevY == 0.0f && prevZ == 0.0f)) {
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

    auto it = idLookup_.find(handle.systemId);
    if (it != idLookup_.end()) {
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
    uint32_t assignedIndex = 0;
    if (!freeFieldIndices_.empty()) {
        assignedIndex = freeFieldIndices_.back();
        freeFieldIndices_.pop_back();
    } else {
        assignedIndex = nextFieldIndex_++;
        if (assignedIndex >= globalFields_.size()) {
            globalFields_.resize(assignedIndex + 1);
        }
    }

    // Initialize slot with disabled field (strength = 0)
    globalFields_[assignedIndex] = ParticleField();

    FieldHandle handle;
    handle.index = assignedIndex;
    return handle;
}

void GPUParticleManager::UnregisterField(const FieldHandle& handle) {
    if (!handle.IsValid()) {
        return;
    }

    if (handle.index < globalFields_.size()) {
        globalFields_[handle.index].strength = 0.0f; // Disable
        freeFieldIndices_.push_back(handle.index);
    }
}

void GPUParticleManager::UpdateFieldData(const FieldHandle& handle, const ParticleField& data) {
    if (handle.IsValid() && handle.index < globalFields_.size()) {
        globalFields_[handle.index] = data;
    }
}

#if defined(USE_IMGUI)
#include <imgui.h>
#endif

int GPUParticleManager::GetTotalEmittersUsed() const {
    int totalEmitters = 0;
    for (const auto& pair : systems_) {
        // nextIndex はこれまで割り当てた最大のインデックス（サイズ）。そこから解放済みの数を引く
        totalEmitters += (int)(pair.second.nextIndex - pair.second.freeIndices.size());
    }
    return totalEmitters;
}

void GPUParticleManager::Debug() {
#if defined(USE_IMGUI)
    if (ImGui::BeginTabItem("GPU Particle Manager")) {
        if (ImGui::BeginChild("GPUParticleScroll", ImVec2(0, 0), false)) {
            ImGui::TextDisabled("(Global system stats moved to TelemetryMonitor)");
            ImGui::Separator();

            ImGui::Spacing();
            ImGui::Text("System Details per Texture");
            for (auto& pair : systems_) {
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
