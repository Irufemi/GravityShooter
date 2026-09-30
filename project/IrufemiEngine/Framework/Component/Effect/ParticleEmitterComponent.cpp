#include "Framework/Component/Effect/ParticleEmitterComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/Utility/Log.h"
#include <iostream>

ParticleEmitterComponent::ParticleEmitterComponent() {
    particleObj_ = std::make_unique<ParticleObject>();
}

ParticleEmitterComponent::~ParticleEmitterComponent() {}

void ParticleEmitterComponent::OnRegisterProperties() {
    if (particleObj_) {
        particleObj_->RegisterProperties(this);
    }
}

void ParticleEmitterComponent::Initialize() {
    OnAwake();
    OnSpawned();
}

void ParticleEmitterComponent::OnAwake() {
    if (particleObj_) {
        particleObj_->Initialize();
    }
}

void ParticleEmitterComponent::OnSpawned() {
    if (particleObj_ && GetTransform()) {
        particleObj_->SetPosition(GetTransform()->GetWorldPosition());
    }
}

void ParticleEmitterComponent::Update() {
    if (GetTransform()) {
        particleObj_->SetPosition(GetTransform()->GetWorldPosition());
    }

    particleObj_->Update();
}

void ParticleEmitterComponent::Draw() {}

void ParticleEmitterComponent::Play() {
    particleObj_->Play();
}

void ParticleEmitterComponent::Restart(bool withChildren) {
    if (particleObj_) {
        // GPU側に放出リクエストを送る前に、最新のワールド座標を即座に反映する
        if (GetTransform()) {
            particleObj_->SetPosition(GetTransform()->GetWorldPosition());
        }

        particleObj_->Restart();
    }

    if (withChildren && gameObject_) {
        auto emitters = gameObject_->GetComponentsInChildren<ParticleEmitterComponent>();
        for (auto childEmitter : emitters) {
            if (childEmitter != this) {
                childEmitter->Restart(false);
            }
        }
    }
}

void ParticleEmitterComponent::Stop() {
    particleObj_->Stop();
}

void ParticleEmitterComponent::EmitBurst(int count) {
    particleObj_->EmitBurst(count);
}

nlohmann::json ParticleEmitterComponent::Serialize() {
    nlohmann::json j = Component::Serialize();
    if (particleObj_) {
        nlohmann::json particleJson;
        particleObj_->Serialize(particleJson);
        j["ParticleData"] = particleJson;
    }
    return j;
}

void ParticleEmitterComponent::Deserialize(const nlohmann::json& j) {
    Component::Deserialize(j);
    if (j.contains("ParticleData") && particleObj_) {
        particleObj_->Deserialize(j["ParticleData"]);
    }
}

std::shared_ptr<Component> ParticleEmitterComponent::Clone() {
    auto clone = std::make_shared<ParticleEmitterComponent>();
    clone->CopyPropertiesFrom(this);
    if (this->particleObj_) {
        clone->particleObj_->CopyFrom(*this->particleObj_);
    }
    return clone;
}
