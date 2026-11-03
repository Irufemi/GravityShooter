#include "Environment/DestructibleEnvironmentComponent.h"
#include "Environment/DebrisManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Core/Math/Random/Random.h"

#include "Effects/EffectManagerComponent.h"
#include "Environment/DebrisComponent.h"

void DestructibleEnvironmentComponent::Initialize() {
    debrisManager_ = nullptr;
    effectManager_ = nullptr;
    hp_ = 1;
}

void DestructibleEnvironmentComponent::Start() {
    // 起動時に先行キャッシュを試みる
    GetDebrisManager();
    GetEffectManager();
}

DebrisManagerComponent* DestructibleEnvironmentComponent::GetDebrisManager() {
    if (debrisManager_) {
        return debrisManager_;
    }
    if (gameObject_) {
        if (auto scene = gameObject_->GetScene()) {
            if (auto managerObj = scene->FindGameObject("DebrisManager")) {
                debrisManager_ = managerObj->GetComponent<DebrisManagerComponent>();
            }
        }
    }
    return debrisManager_;
}

EffectManagerComponent* DestructibleEnvironmentComponent::GetEffectManager() {
    if (!effectManager_) {
        effectManager_ = EffectManagerComponent::GetInstance();
        if (!effectManager_ && gameObject_) {
            if (auto scene = gameObject_->GetScene()) {
                if (auto effectObj = scene->FindGameObject("EffectManager")) {
                    effectManager_ = effectObj->GetComponent<EffectManagerComponent>();
                }
            }
        }
    }
    return effectManager_;
}

void DestructibleEnvironmentComponent::TakeDamage(int damage) {
    if (hp_ <= 0) {
        return; // 既に破壊されている
    }

    hp_ -= damage;

    if (hp_ <= 0) {
        // 破壊イベント発火
        auto transform = gameObject_->GetTransform();
        auto debrisMgr = GetDebrisManager();
        if (transform && debrisMgr) {
            Irufemi::Vector3 pos = transform->GetWorldPosition();

            // 破壊エフェクト（煙・粉塵）
            if (auto effectMgr = GetEffectManager()) {
                effectMgr->PlayEffect("Dust", pos);
            }

            // 瓦礫のスポーン（散らばるように）
            for (int i = 0; i < debrisSpawnCount_; ++i) {
                auto debris = debrisMgr->GetDebris();
                if (debris) {
                    if (auto debrisTransform = debris->GetTransform()) {
                        // ランダムなオフセットと上方向への位置調整
                        Irufemi::Vector3 offset = {Irufemi::Random::GeneratorFloat(-2.0f, 2.0f),
                                                   Irufemi::Random::GeneratorFloat(2.0f, 6.0f),
                                                   Irufemi::Random::GeneratorFloat(-2.0f, 2.0f)};
                        debrisTransform->SetWorldPosition(pos + offset);
                    }

                    if (auto comp = debris->GetComponent<DebrisComponent>()) {
                        comp->SetState(DebrisState::Idle, true); // オーラ消灯・状態同期を強制
                        comp->SetTarget(std::weak_ptr<GameObject>());
                    }
                }
            }
        }

        // 自身を非アクティブ化して消滅
        gameObject_->SetIsActive(false);
    }
}
