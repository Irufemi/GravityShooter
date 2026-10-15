#include "Framework/Component/Effect/ParticleFieldComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Core/System/IrufemiEngine.h"
#include <cstring>
ParticleFieldComponent::ParticleFieldComponent() {
    fieldData_.type = 1; // Default to Point Attractor
    fieldType_ = static_cast<int>(fieldData_.type);
    fieldData_.strength = 10.0f;
    fieldData_.range = 50.0f;
    fieldData_.falloff = 1.0f;
    fieldData_.direction = {0.0f, -1.0f, 0.0f};
    fieldData_.axis = {0.0f, 1.0f, 0.0f};
}

ParticleFieldComponent::~ParticleFieldComponent() {
    OnDestroy();
}

void ParticleFieldComponent::Initialize() {
    Start();
}

void ParticleFieldComponent::Start() {
    if (!fieldHandle_.IsValid()) {
        if (auto engine = GetEngine()) {
            gpuParticleManager_ = engine->GetGPUParticleManager();
        }
        if (gpuParticleManager_) {
            fieldHandle_ = gpuParticleManager_->RegisterField();
        }
    }
}

void ParticleFieldComponent::OnDestroy() {
    if (fieldHandle_.IsValid() && gpuParticleManager_) {
        gpuParticleManager_->UnregisterField(fieldHandle_);
        fieldHandle_ = {};
    }
}

void ParticleFieldComponent::Update() {
    if (GetTransform()) {
        fieldData_.position = GetTransform()->GetWorldPosition();
    }

    if (!gpuParticleManager_) {
        if (auto engine = GetEngine()) {
            gpuParticleManager_ = engine->GetGPUParticleManager();
        }
    }

    fieldData_.type = static_cast<uint32_t>(fieldType_);

    // ダーティチェック: パラメータや位置に変更があった場合のみ GPU へバッファ転送
    const bool isDirty = isFirstUpdate_ ||
                         (std::memcmp(&lastFieldData_, &fieldData_, sizeof(ParticleField)) != 0) ||
                         (lastFieldType_ != fieldType_);

    if (isDirty && fieldHandle_.IsValid() && gpuParticleManager_) {
        gpuParticleManager_->UpdateFieldData(fieldHandle_, fieldData_);
        lastFieldData_ = fieldData_;
        lastFieldType_ = fieldType_;
        isFirstUpdate_ = false;
    }
}

void ParticleFieldComponent::OnRegisterProperties() {
    static const std::vector<std::string> fieldTypeNames = {"Directional", "Point Attractor", "Vortex"};
    RegisterEnum("Field Type", &fieldType_, fieldTypeNames);
    RegisterProperty("Strength", &fieldData_.strength).SetMinMax(-1000.0f, 1000.0f);
    RegisterProperty("Effect Range", &fieldData_.range).SetMinMax(0.0f, 10000.0f);
    RegisterProperty("Falloff", &fieldData_.falloff).SetMinMax(0.0f, 10.0f);
    RegisterProperty("Direction", &fieldData_.direction);
    RegisterProperty("Axis", &fieldData_.axis);
}

nlohmann::json ParticleFieldComponent::Serialize() {
    nlohmann::json j;
    j["type"] = fieldData_.type;
    j["strength"] = fieldData_.strength;
    j["range"] = fieldData_.range;
    j["falloff"] = fieldData_.falloff;
    j["direction"] = {fieldData_.direction.x, fieldData_.direction.y, fieldData_.direction.z};
    j["axis"] = {fieldData_.axis.x, fieldData_.axis.y, fieldData_.axis.z};
    return j;
}

void ParticleFieldComponent::Deserialize(const nlohmann::json& j) {
    if (j.contains("type")) {
        fieldData_.type = j["type"];
        fieldType_ = static_cast<int>(fieldData_.type);
    }
    if (j.contains("strength")) {
        fieldData_.strength = j["strength"];
    }
    if (j.contains("range")) {
        fieldData_.range = j["range"];
    }
    if (j.contains("falloff")) {
        fieldData_.falloff = j["falloff"];
    }
    if (j.contains("direction")) {
        fieldData_.direction.x = j["direction"][0];
        fieldData_.direction.y = j["direction"][1];
        fieldData_.direction.z = j["direction"][2];
    }
    if (j.contains("axis")) {
        fieldData_.axis.x = j["axis"][0];
        fieldData_.axis.y = j["axis"][1];
        fieldData_.axis.z = j["axis"][2];
    }
}

std::shared_ptr<Component> ParticleFieldComponent::Clone() {
    auto clone = std::make_shared<ParticleFieldComponent>();
    clone->CopyPropertiesFrom(this);
    clone->fieldData_ = this->fieldData_;
    return clone;
}
