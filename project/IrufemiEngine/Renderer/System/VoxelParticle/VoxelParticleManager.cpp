#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/Pipeline/PSOManager.h"
#include <vector>

void VoxelParticleManager::Initialize(IrufemiEngine* engine) {
    engine_ = engine;
}

VoxelParticleManager::EmitterHandle VoxelParticleManager::RegisterEmitter(const std::string& modelName,
                                                                          const Irufemi::Vector3Int& resolution) {
    SystemKey key{modelName, resolution};
    auto& context = systems_[key];

    if (!context.system) {
        context.systemId = nextSystemId_++;
        context.system = std::make_unique<VoxelParticleSystem>();
        VoxelParticleSystem::SetEngine(engine_);
        context.system->Initialize(modelName, resolution);
        context.nextIndex = 0;
        idLookup_[context.systemId] = &context;
    }

    uint32_t index = 0;
    if (!context.freeIndices.empty()) {
        index = context.freeIndices.back();
        context.freeIndices.pop_back();
    } else {
        if (context.nextIndex >= context.system->GetMaxInstances()) {
            return EmitterHandle{}; // 上限に達した場合は無効なハンドルを返す
        }
        index = context.nextIndex++;
    }

    if (index >= context.slotGenerations.size()) {
        context.slotGenerations.resize(index + 1, 1);
    }

    EmitterHandle handle;
    handle.systemId = context.systemId;
    handle.emitterIndex = static_cast<uint16_t>(index);
    handle.generation = context.slotGenerations[index];
    return handle;
}

void VoxelParticleManager::UnregisterEmitter(const EmitterHandle& handle) {
    if (!handle.IsValid()) {
        return;
    }

    auto it = idLookup_.find(handle.systemId);
    if (it != idLookup_.end()) {
        auto* context = it->second;
        if (handle.emitterIndex < context->slotGenerations.size() &&
            context->slotGenerations[handle.emitterIndex] == handle.generation) {
            if (context->system && handle.emitterIndex < context->system->GetMaxInstances()) {
                VoxelEmitter emptyData;
                emptyData.emit = 0;
                emptyData.lifeTime = 0.0f;
                context->system->UpdateEmitterData(handle.emitterIndex, emptyData);
            }
            // 世代番号をインクリメントして古いハンドルを無効化
            context->slotGenerations[handle.emitterIndex]++;
            context->freeIndices.push_back(handle.emitterIndex);
        }
    }
}

void VoxelParticleManager::Clear() {
    // 実行中のワンショットエミッターをすべて即座に解放
    for (auto& shot : oneShots_) {
        UnregisterEmitter(shot.handle);
    }
    oneShots_.clear();

    // 登録済みの全システムの状態をリセット
    for (auto& pair : systems_) {
        if (pair.second.system) {
            uint32_t maxInstances = pair.second.system->GetMaxInstances();
            for (uint32_t i = 0; i < maxInstances; ++i) {
                VoxelEmitter emptyData;
                emptyData.emit = 0;
                emptyData.lifeTime = 0.0f;
                pair.second.system->UpdateEmitterData(i, emptyData);
            }

            // インデックスおよび世代を更新して既存ハンドルをすべて無効化
            for (auto& gen : pair.second.slotGenerations) {
                gen++;
            }
            pair.second.freeIndices.clear();
            pair.second.nextIndex = 0;
        }
    }
}

void VoxelParticleManager::UpdateEmitterData(const EmitterHandle& handle, const VoxelEmitter& data) {
    if (!handle.IsValid()) {
        return;
    }

    auto it = idLookup_.find(handle.systemId);
    if (it != idLookup_.end()) {
        auto* context = it->second;
        if (handle.emitterIndex < context->slotGenerations.size() &&
            context->slotGenerations[handle.emitterIndex] == handle.generation && context->system) {
            context->system->UpdateEmitterData(handle.emitterIndex, data);
        }
    }
}

const VoxelEmitter& VoxelParticleManager::GetEmitterData(const EmitterHandle& handle) const {
    static const VoxelEmitter dummy{};
    if (!handle.IsValid()) {
        return dummy;
    }

    auto it = idLookup_.find(handle.systemId);
    if (it != idLookup_.end()) {
        const auto* context = it->second;
        if (handle.emitterIndex < context->slotGenerations.size() &&
            context->slotGenerations[handle.emitterIndex] == handle.generation && context->system) {
            return context->system->GetEmitterData(handle.emitterIndex);
        }
    }
    return dummy;
}

void VoxelParticleManager::Update(float deltaTime) {
    for (auto& pair : systems_) {
        if (pair.second.system) {
            pair.second.system->Update(deltaTime);
        }
    }

    std::erase_if(oneShots_, [this, deltaTime](auto& shot) {
        if (shot.emitTimer > 0.0f) {
            shot.emitTimer -= deltaTime;
            if (shot.emitTimer <= 0.0f) {
                VoxelEmitter data = GetEmitterData(shot.handle);
                data.emit = 0;
                UpdateEmitterData(shot.handle, data);
            }
        }

        shot.lifeTimer -= deltaTime;
        if (shot.lifeTimer <= 0.0f) {
            UnregisterEmitter(shot.handle);
            return true;
        }
        return false;
    });
}

void VoxelParticleManager::Draw() {
    for (auto& pair : systems_) {
        if (pair.second.system) {
            engine_->SetBlend(Irufemi::BlendMode::kBlendModeNormal);
            engine_->SetDepthWrite(PSOManager::DepthWrite::Enable);
            engine_->SetCull(PSOManager::CullMode::Back);
            pair.second.system->Draw();
        }
    }
}

void VoxelParticleManager::ReservePool(const std::string& modelName, const Irufemi::Vector3Int& resolution,
                                       int preAllocateCount) {
    auto handle = RegisterEmitter(modelName, resolution);
    UnregisterEmitter(handle);
}

bool VoxelParticleManager::IsAllReady() const {
    for (const auto& pair : systems_) {
        if (pair.second.system && !pair.second.system->IsReady()) {
            return false;
        }
    }
    return true;
}

void VoxelParticleManager::EnsureAllInitialized() {
    for (auto& pair : systems_) {
        if (pair.second.system) {
            pair.second.system->EnsureInitialized();
        }
    }
}

void VoxelParticleManager::WarmUp() {
    EnsureAllInitialized();
    // 登録済みの全システムの状態をリセット・同期（モデル読み込みは各コンポーネント・プレハブのStart()に委任）
    for (auto& pair : systems_) {
        if (pair.second.system) {
            for (auto& gen : pair.second.slotGenerations) {
                gen++;
            }
            pair.second.freeIndices.clear();
            pair.second.nextIndex = 0;
        }
    }
}

void VoxelParticleManager::PlayExplosion(const std::string& modelName, const Irufemi::Vector3& worldPos,
                                         const Irufemi::Vector3& velocity, const Irufemi::Vector3& rotate,
                                         const Irufemi::Vector3& scale, const VoxelEmitter& params,
                                         const Irufemi::Vector3Int& resolution) {
    auto handle = RegisterEmitter(modelName, resolution);
    if (!handle.IsValid()) {
        return; // 制限オーバーで取得できなかった場合は処理しない
    }

    VoxelEmitter explosion = params;
    explosion.emit = 1;
    explosion.emitPosition = worldPos;
    explosion.baseVelocity = velocity;
    explosion.rotate = rotate;
    explosion.scale = scale;
    UpdateEmitterData(handle, explosion);

    OneShotEmitter shot{};
    shot.handle = handle;
    shot.emitTimer = 0.1f;
    shot.lifeTimer = params.lifeTime + 0.5f;
    oneShots_.push_back(shot);
}
