#include "Combat/Boss/BossComponent.h"
#include "Combat/Boss/BossStateIdle.h"
#include "Combat/Boss/BossStateDestroyed.h"
#include "Core/Math/MathFunction.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Environment/DebrisManagerComponent.h"
#include "Environment/DebrisComponent.h"
#include "Combat/EnemyBeamComponent.h"
#include "Combat/DroneManagerComponent.h"
#include "Combat/BossBulletManagerComponent.h"
#include "Combat/Boss/BossDamageVisualizerComponent.h"
#include "Player/TargetableComponent.h"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include "Core/Utility/JsonUtility.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/ContainerUtility.h"

void BossComponent::LoadStatusFromJson() {
    if (statusDataPath_.empty()) {
        return;
    }

    nlohmann::json j;
    if (!Irufemi::JsonUtility::LoadFromFile(statusDataPath_, j)) {
        Log::OutPutLog(std::cout, "[BossComponent] Failed to load status: " + statusDataPath_ + "\n");
        return;
    }

    if (j.contains("maxHp")) {
        maxHp_ = j["maxHp"].get<float>();
        hp_ = maxHp_;
    }
    if (j.contains("maxShieldCount")) {
        maxShieldCount_ = j["maxShieldCount"].get<int>();
    }
    if (j.contains("shieldRadius")) {
        shieldRadius_ = j["shieldRadius"].get<float>();
    }
    if (j.contains("beamInterval")) {
        beamInterval_ = j["beamInterval"].get<float>();
    }
    if (j.contains("beamRange")) {
        beamRange_ = j["beamRange"].get<float>();
    }
}

BossComponent::BossComponent() {}

void BossComponent::Initialize() {
    auto targetable = gameObject_->GetComponent<TargetableComponent>();
    if (!targetable) {
        auto comp = gameObject_->AddComponent<TargetableComponent>();
        targetable = comp.get();
    }
    if (targetable) {
        targetable->SetTargetablePredicate([this]() { return IsCoreExposed(); });
    }

    LoadStatusFromJson();
    hp_ = maxHp_;
    isShieldsInitialized_ = false;

    if (gameObject_) {
        auto collider = gameObject_->GetComponent<SphereColliderComponent>();
        if (!collider) {
            collider = gameObject_->AddComponent<SphereColliderComponent>().get();
        }
        if (collider) {
            collider->SetTrigger(true);
        }
    }

    if (gameObject_) {
        beamComponent_ = gameObject_->GetComponent<EnemyBeamComponent>();
        if (!beamComponent_) {
            auto comp = gameObject_->AddComponent<EnemyBeamComponent>();
            beamComponent_ = comp.get();
            beamComponent_->Initialize();
        }

        if (!gameObject_->GetComponent<BossDamageVisualizerComponent>()) {
            gameObject_->AddComponent<BossDamageVisualizerComponent>();
        }
    }
    beamTimer_ = 0.0f;

    // 初期ステートの設定
    ChangeState(std::make_unique<BossStateIdle>());
}

void BossComponent::Start() {
    if (!gameObject_) {
        return;
    }

    if (auto col = gameObject_->GetComponent<SphereColliderComponent>()) {
        col->SetDebugCategory(DebugCategory::Combat);
        col->SetDebugCustomColor(Irufemi::Vector4{1.0f, 0.0f, 1.0f, 1.0f});
    }

    auto scene = gameObject_->GetScene();
    if (scene) {
        auto container = scene->FindGameObject("BossContainer");
        if (container) {
            bossContainer_ = container;
            auto droneObj = scene->FindGameObject("BossDroneManager");
            if (droneObj) {
                droneManager_ = droneObj->GetComponent<DroneManagerComponent>();
            }
            auto bulletObj = scene->FindGameObject("BossBulletManager");
            if (bulletObj) {
                bulletManager_ = bulletObj->GetComponent<BossBulletManagerComponent>();
            }
        }

        auto managerObj = scene->FindGameObject("DebrisManager");
        if (managerObj) {
            debrisManager_ = managerObj->GetComponent<DebrisManagerComponent>();
            if (debrisManager_) {
                auto setupDebris = [this](std::shared_ptr<GameObject> debrisObj) {
                    auto debrisComp = debrisObj->GetComponent<DebrisComponent>();
                    if (debrisComp) {
                        debrisComp->SetTarget(gameObject_->shared_from_this());
                        debrisComp->SetState(DebrisState::BossOrbiting);
                    }
                    shields_.push_back(debrisObj);
                };

                for (int i = 0; i < maxShieldCount_; ++i) {
                    auto debrisObj = debrisManager_->GetDebris();
                    if (debrisObj) {
                        setupDebris(debrisObj);
                        initialShieldsSpawned_++;
                    }
                }
                isShieldsInitialized_ = true;

                if (droneManager_ && bulletManager_) {
                    droneManager_->DeployDrones(gameObject_->shared_from_this(), 10, bulletManager_);
                }
            }
        }
    }
}

void BossComponent::Update() {
    if (!gameObject_) {
        return;
    }

    if (currentState_) {
        currentState_->Update(this);
    }
}

void BossComponent::OnRegisterProperties() {
    RegisterProperty("Status Data Path", &statusDataPath_);
    RegisterProperty("Beam Offset Z", &beamOffsetZ_);
    RegisterProperty("Beam Offset Y", &beamOffsetY_);
}

std::shared_ptr<GameObject> BossComponent::ExtractDebris() {
    if (shields_.empty()) {
        return nullptr;
    }

    auto debris = shields_.back();
    shields_.pop_back();

    if (debris) {
        auto debrisComp = debris->GetComponent<DebrisComponent>();
        if (debrisComp) {
            debrisComp->SetState(DebrisState::Idle);
            debrisComp->SetTarget(std::weak_ptr<GameObject>());
        }
    }
    return debris;
}

void BossComponent::RemoveShield(std::shared_ptr<GameObject> shield) {
    Irufemi::Container::EraseSwap(shields_, shield);
}

void BossComponent::TakeDamage(float damage) {
    if (currentState_) {
        currentState_->OnTakeDamage(this, damage);
    }
}

void BossComponent::ChangeState(std::unique_ptr<IBossState> newState) {
    if (currentState_) {
        currentState_->Exit(this);
    }
    currentState_ = std::move(newState);
    if (currentState_) {
        currentState_->Enter(this);
    }
}

void BossComponent::NotifyDamageTaken(float damage) {
    for (auto& listener : onDamageTakenListeners_) {
        if (listener) {
            listener(damage);
        }
    }
}

void BossComponent::NotifyBossDied() {
    if (onBossDied_) {
        onBossDied_();
    }
    for (auto& listener : onBossDiedListeners_) {
        if (listener) {
            listener();
        }
    }
}

void BossComponent::NotifyDeathSequenceFinished() {
    if (onDeathSequenceFinished_) {
        onDeathSequenceFinished_();
    }
}

void BossComponent::UpdateBeamAttack(float deltaTime) {
    if (!beamComponent_ || !gameObject_) {
        return;
    }

    if (!beamComponent_->IsActive()) {
        beamTimer_ += deltaTime;
        if (beamTimer_ >= beamInterval_) {
            beamTimer_ = 0.0f;

            if (auto myTrans = GetTransform()) {
                // ボスの前面（プレイヤー側＝ローカル -Z方向）および口元（ローカルY）のオフセット
                Irufemi::Vector3 localMuzzle = {0.0f, beamOffsetY_, -beamOffsetZ_};
                Irufemi::Vector3 startPos = Irufemi::Math::Transform(localMuzzle, myTrans->GetWorldMatrix());

                Irufemi::Vector3 forward = -myTrans->GetWorldForward();
                Irufemi::Vector3 targetPos = Irufemi::Math::Add(startPos, Irufemi::Math::Multiply(beamRange_, forward));
                beamComponent_->Fire(startPos, targetPos);
            }
        }
    }
}

void BossComponent::ApplyCoreDamage(float damage) {
    hp_ -= damage;

    std::string dmgLog = "Boss took damage! HP: " + std::to_string(hp_) + "\n";
    Log::OutPutLog(std::cout, dmgLog);

    NotifyDamageTaken(damage);

    if (hp_ <= 0.0f) {
        hp_ = 0.0f;
        ChangeState(std::make_unique<BossStateDestroyed>());
    }
}

void BossComponent::SpawnDebrisCluster(int count, float radius) {
    if (debrisManager_ && gameObject_) {
        Irufemi::Vector3 bossPos = gameObject_->GetTransform()->GetWorldPosition();
        debrisManager_->SpawnDebrisCluster(bossPos, count, radius);
    }
}
