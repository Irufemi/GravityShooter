#include "Framework/Component/Utility/LifetimeComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Core/System/IrufemiEngine.h"

namespace Irufemi {

void LifetimeComponent::OnRegisterProperties() {
    timeoutActionInt_ = static_cast<int>(timeoutAction_);
    RegisterProperty("Life Time", &lifeTime_);
    RegisterEnum("Timeout Action", &timeoutActionInt_, {"0: Destroy", "1: Disable"}).OnChanged([this]() {
        timeoutAction_ = static_cast<TimeoutAction>(timeoutActionInt_);
    });
}

void LifetimeComponent::Initialize() {
    currentLifeTime_ = 0.0f;
}

void LifetimeComponent::OnSpawned() {
    currentLifeTime_ = 0.0f;
}

void LifetimeComponent::OnEnable() {
    currentLifeTime_ = 0.0f;
}

void LifetimeComponent::Update() {
    auto engine = GetEngine();
    float dt = engine ? engine->GetGameDeltaTime() : 0.0f;
    currentLifeTime_ += dt;
    if (currentLifeTime_ >= lifeTime_) {
        if (timeoutAction_ == TimeoutAction::Destroy) {
            gameObject_->Destroy();
        } else {
            gameObject_->SetIsActive(false);
        }
    }
}

nlohmann::json LifetimeComponent::Serialize() {
    nlohmann::json j = Component::Serialize();
    j["lifeTime"] = lifeTime_;
    j["timeoutAction"] = static_cast<int>(timeoutAction_);
    return j;
}

void LifetimeComponent::Deserialize(const nlohmann::json& j) {
    Component::Deserialize(j);
    if (j.contains("lifeTime")) {
        lifeTime_ = j["lifeTime"].get<float>();
    }
    if (j.contains("timeoutAction")) {
        timeoutAction_ = static_cast<TimeoutAction>(j["timeoutAction"].get<int>());
        timeoutActionInt_ = static_cast<int>(timeoutAction_);
    }
}

} // namespace Irufemi
