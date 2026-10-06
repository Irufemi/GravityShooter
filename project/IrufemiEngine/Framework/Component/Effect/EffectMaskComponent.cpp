#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Renderer/SkinnedMeshRendererComponent.h"
#include "Core/System/IrufemiEngine.h"

EffectMaskComponent::EffectMaskComponent() {}

EffectMaskComponent::~EffectMaskComponent() {
    OnDestroy();
}

void EffectMaskComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void EffectMaskComponent::OnAwake() {}

void EffectMaskComponent::OnSpawned() {
    if (auto* engine = GetEngine()) {
        cachedPostProcessManager_ = engine->GetPostProcessManager();
    }
}

void EffectMaskComponent::OnDestroy() {
    if (cachedPostProcessManager_ && ownerId_ != 0) {
        cachedPostProcessManager_->UnregisterCustomEffectParams(ownerId_);
        ownerId_ = 0;
        assignedSlot_ = 0;
    }
}

void EffectMaskComponent::SetCustomParams(const PostProcessManager::CustomEffectParams& params) {
    customParams_ = params;
    if (enableEffectMask_ && customEffectType_ > 0) {
        if (!cachedPostProcessManager_) {
            if (auto* engine = GetEngine()) {
                cachedPostProcessManager_ = engine->GetPostProcessManager();
            }
        }
        if (cachedPostProcessManager_) {
            if (ownerId_ == 0) {
                ownerId_ = gameObject_ ? gameObject_->GetInstanceID() : reinterpret_cast<uint64_t>(this);
            }
            assignedSlot_ = cachedPostProcessManager_->RegisterCustomEffectParams(ownerId_, customParams_);
            cachedEffectParam_ = static_cast<float>(assignedSlot_) / 255.0f;
        }
    }
}

void EffectMaskComponent::Update() {
    if (enableEffectMask_ && customEffectType_ > 0) {
        if (!cachedPostProcessManager_) {
            if (auto* engine = GetEngine()) {
                cachedPostProcessManager_ = engine->GetPostProcessManager();
            }
        }

        if (cachedPostProcessManager_) {
            ownerId_ = gameObject_ ? gameObject_->GetInstanceID() : reinterpret_cast<uint64_t>(this);
            assignedSlot_ = cachedPostProcessManager_->RegisterCustomEffectParams(ownerId_, customParams_);
            cachedEffectParam_ = static_cast<float>(assignedSlot_) / 255.0f;
        }
    } else {
        cachedEffectParam_ = 0.0f;
    }

    const bool isDirty =
        (lastEnable_ != enableEffectMask_ || lastType_ != customEffectType_ || lastParam_ != cachedEffectParam_);
    if (isDirty && gameObject_) {
        if (auto meshRenderer = gameObject_->GetComponent<MeshRendererComponent>()) {
            meshRenderer->SetEnableEffectMask(enableEffectMask_);
            meshRenderer->SetCustomEffectType(customEffectType_);
            meshRenderer->SetCustomEffectParam(cachedEffectParam_);
        }
        if (auto skinnedRenderer = gameObject_->GetComponent<SkinnedMeshRendererComponent>()) {
            skinnedRenderer->SetEnableEffectMask(enableEffectMask_);
            skinnedRenderer->SetCustomEffectType(customEffectType_);
            skinnedRenderer->SetCustomEffectParam(cachedEffectParam_);
        }
        lastEnable_ = enableEffectMask_;
        lastType_ = customEffectType_;
        lastParam_ = cachedEffectParam_;
    }
}

void EffectMaskComponent::OnRegisterProperties() {
    RegisterProperty("Enable Effect Mask", &enableEffectMask_);
    RegisterProperty("Custom Effect Type", &customEffectType_);
    RegisterProperty("Color 1", &customParams_.color1);
    RegisterProperty("Color 2", &customParams_.color2);
    RegisterProperty("Param 1", &customParams_.param1);
    RegisterProperty("Param 2", &customParams_.param2);
    RegisterProperty("Param 3", &customParams_.param3);
    RegisterProperty("Param 4", &customParams_.param4);
}

nlohmann::json EffectMaskComponent::Serialize() {
    nlohmann::json j;
    j["enableEffectMask"] = enableEffectMask_;
    j["customEffectType"] = customEffectType_;
    j["customParams"]["color1"] = {customParams_.color1.x, customParams_.color1.y, customParams_.color1.z,
                                   customParams_.color1.w};
    j["customParams"]["color2"] = {customParams_.color2.x, customParams_.color2.y, customParams_.color2.z,
                                   customParams_.color2.w};
    j["customParams"]["param1"] = customParams_.param1;
    j["customParams"]["param2"] = customParams_.param2;
    j["customParams"]["param3"] = customParams_.param3;
    j["customParams"]["param4"] = customParams_.param4;
    return j;
}

void EffectMaskComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("enableEffectMask")) {
        enableEffectMask_ = j["enableEffectMask"];
    }
    if (j.contains("customEffectType")) {
        customEffectType_ = j["customEffectType"];
    }
    // Backward compatibility
    if (j.contains("customEffectParam")) {
        customParams_.param1 = j["customEffectParam"];
    }

    if (j.contains("customParams")) {
        auto& p = j["customParams"];
        if (p.contains("color1")) {
            customParams_.color1 = {p["color1"][0], p["color1"][1], p["color1"][2], p["color1"][3]};
        }
        if (p.contains("color2")) {
            customParams_.color2 = {p["color2"][0], p["color2"][1], p["color2"][2], p["color2"][3]};
        }
        if (p.contains("param1")) {
            customParams_.param1 = p["param1"];
        }
        if (p.contains("param2")) {
            customParams_.param2 = p["param2"];
        }
        if (p.contains("param3")) {
            customParams_.param3 = p["param3"];
        }
        if (p.contains("param4")) {
            customParams_.param4 = p["param4"];
        }
    }
}

std::shared_ptr<Component> EffectMaskComponent::Clone() {
    auto clone = std::make_shared<EffectMaskComponent>();
    clone->CopyPropertiesFrom(this);
    clone->enableEffectMask_ = this->enableEffectMask_;
    clone->customEffectType_ = this->customEffectType_;
    clone->customParams_ = this->customParams_;
    clone->cachedEffectParam_ = this->cachedEffectParam_;
    return clone;
}
