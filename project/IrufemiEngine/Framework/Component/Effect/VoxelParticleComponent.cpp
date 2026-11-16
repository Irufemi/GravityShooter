#include "Framework/Component/Effect/VoxelParticleComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"

#include <iostream>

using namespace Irufemi;

VoxelParticleComponent::VoxelParticleComponent() {}

VoxelParticleComponent::~VoxelParticleComponent() {}

void VoxelParticleComponent::Initialize() {
    OnAwake();
}

void VoxelParticleComponent::OnAwake() {
    cachedModelName_ = GetTargetModelName();
}

void VoxelParticleComponent::Start() {
    if (cachedModelName_.empty()) {
        cachedModelName_ = GetTargetModelName();
    }

    if (!cachedModelName_.empty()) {
        IrufemiEngine* engine = nullptr;
        if (auto* go = GetGameObject()) {
            if (auto* scene = go->GetScene()) {
                engine = scene->GetEngine();
            }
        }
        if (engine) {
            if (auto manager = engine->GetVoxelParticleManager()) {
                manager->ReservePool(cachedModelName_, resolution_, preAllocateCount_);
                isInitialized_ = true;
            }
        }
    }
}

void VoxelParticleComponent::Update() {
    // 描画・更新はVoxelParticleManagerが一括で行うため、ここでは特に何もしない
}

void VoxelParticleComponent::Draw() {
    // 描画・更新はVoxelParticleManagerが一括で行うため、ここでは特に何もしない
}

std::string VoxelParticleComponent::GetTargetModelName() {
    if (!overrideModelName_.empty()) {
        return overrideModelName_;
    }

    if (auto* go = GetGameObject()) {
        if (auto meshRenderer = go->GetComponent<MeshRendererComponent>()) {
            return meshRenderer->GetModelName();
        }
        if (auto batchRenderer = go->GetComponent<ModelBatchRendererComponent>()) {
            return batchRenderer->GetModelName();
        }
    }
    return "";
}

void VoxelParticleComponent::Emit() {
    // 拡張用：Emit（ポタポタ落ちるような挙動等）が必要になった場合はここに実装
}

void VoxelParticleComponent::Explode(const Irufemi::Vector3& velocity, const Irufemi::Vector3& rotate,
                                     const Irufemi::Vector3& scale) {
    if (!isInitialized_) {
        return;
    }

    IrufemiEngine* engine = nullptr;
    if (auto* go = GetGameObject()) {
        if (auto* scene = go->GetScene()) {
            engine = scene->GetEngine();
        }
    }

    if (engine) {
        if (auto manager = engine->GetVoxelParticleManager()) {
            Irufemi::Vector3 worldPos = {0, 0, 0};
            if (auto* go = GetGameObject()) {
                if (auto transform = go->GetComponent<TransformComponent>()) {
                    worldPos = transform->GetWorldPosition();
                }
            }
            manager->PlayExplosion(cachedModelName_, worldPos, velocity, rotate, scale, emitterParams_, resolution_);
        }
    }
}

nlohmann::json VoxelParticleComponent::Serialize() {
    nlohmann::json j;
    j["overrideModelName"] = overrideModelName_;
    j["resolution"] = {resolution_.x, resolution_.y, resolution_.z};
    j["preAllocateCount"] = preAllocateCount_;

    j["particleType"] = static_cast<uint32_t>(emitterParams_.particleType);
    j["lifeTime"] = emitterParams_.lifeTime;
    j["gravity"] = emitterParams_.gravity;
    j["dispersion"] = emitterParams_.dispersion;
    j["convergence"] = emitterParams_.convergence;
    j["scale"] = {emitterParams_.scale.x, emitterParams_.scale.y, emitterParams_.scale.z};
    j["startColor"] = {emitterParams_.startColor.x, emitterParams_.startColor.y, emitterParams_.startColor.z,
                       emitterParams_.startColor.w};
    j["endColor"] = {emitterParams_.endColor.x, emitterParams_.endColor.y, emitterParams_.endColor.z,
                     emitterParams_.endColor.w};
    j["dissolveEdgeColor"] = {emitterParams_.dissolveEdgeColor.x, emitterParams_.dissolveEdgeColor.y,
                              emitterParams_.dissolveEdgeColor.z, emitterParams_.dissolveEdgeColor.w};

    return j;
}

void VoxelParticleComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("overrideModelName")) {
        overrideModelName_ = j["overrideModelName"];
    }
    if (j.contains("resolution")) {
        resolution_.x = j["resolution"][0];
        resolution_.y = j["resolution"][1];
        resolution_.z = j["resolution"][2];
    }
    if (j.contains("preAllocateCount")) {
        preAllocateCount_ = j["preAllocateCount"];
    }

    if (j.contains("particleType")) {
        emitterParams_.particleType = j["particleType"];
    }
    if (j.contains("lifeTime")) {
        emitterParams_.lifeTime = j["lifeTime"];
    }
    if (j.contains("gravity")) {
        emitterParams_.gravity = j["gravity"];
    }
    if (j.contains("dispersion")) {
        emitterParams_.dispersion = j["dispersion"];
    }
    if (j.contains("convergence")) {
        emitterParams_.convergence = j["convergence"];
    }
    if (j.contains("scale") && j["scale"].is_array() && j["scale"].size() >= 3) {
        emitterParams_.scale.x = j["scale"][0];
        emitterParams_.scale.y = j["scale"][1];
        emitterParams_.scale.z = j["scale"][2];
    }
    if (j.contains("startColor") && j["startColor"].is_array() && j["startColor"].size() >= 4) {
        emitterParams_.startColor.x = j["startColor"][0];
        emitterParams_.startColor.y = j["startColor"][1];
        emitterParams_.startColor.z = j["startColor"][2];
        emitterParams_.startColor.w = j["startColor"][3];
    }
    if (j.contains("endColor") && j["endColor"].is_array() && j["endColor"].size() >= 4) {
        emitterParams_.endColor.x = j["endColor"][0];
        emitterParams_.endColor.y = j["endColor"][1];
        emitterParams_.endColor.z = j["endColor"][2];
        emitterParams_.endColor.w = j["endColor"][3];
    }
    if (j.contains("dissolveEdgeColor") && j["dissolveEdgeColor"].is_array() && j["dissolveEdgeColor"].size() >= 4) {
        emitterParams_.dissolveEdgeColor.x = j["dissolveEdgeColor"][0];
        emitterParams_.dissolveEdgeColor.y = j["dissolveEdgeColor"][1];
        emitterParams_.dissolveEdgeColor.z = j["dissolveEdgeColor"][2];
        emitterParams_.dissolveEdgeColor.w = j["dissolveEdgeColor"][3];
    }
}
