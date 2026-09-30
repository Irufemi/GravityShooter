#include "Combat/EnemyBulletComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Environment/DebrisComponent.h"
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Physics/CollisionManager.h"
#include "Effects/EffectManagerComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/BaseScene.h"

void EnemyBulletComponent::Initialize() {
    lifeTimer_ = 0.0f;
}

void EnemyBulletComponent::Launch(const Irufemi::Vector3& direction, float speed, int damage) {
    velocity_ = {direction.x * speed, direction.y * speed, direction.z * speed};
    damage_ = damage;
    lifeTimer_ = 0.0f;
}

void EnemyBulletComponent::ResetForPool() {
    velocity_ = {0.0f, 0.0f, 0.0f};
    damage_ = 10;
    lifeTimer_ = 0.0f;
}

void EnemyBulletComponent::Deactivate() {
    if (manager_) {
        manager_->ReturnBullet(this);
    } else if (gameObject_) {
        gameObject_->Destroy();
    }
}

void EnemyBulletComponent::Update() {
    if (!gameObject_) {
        return;
    }

    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    float dt = engine->GetGameDeltaTime();
    if (dt <= 0.0f) {
        return;
    }

    lifeTimer_ += dt;
    if (lifeTimer_ >= maxLifeTime_) {
        Deactivate();
        return;
    }

    if (auto transform = GetTransform()) {
        Irufemi::Vector3 pos = transform->GetWorldPosition();
        pos.x += velocity_.x * dt;
        pos.y += velocity_.y * dt;
        pos.z += velocity_.z * dt;
        transform->SetWorldPosition(pos);
    }
}

void EnemyBulletComponent::OnCollisionEnter(GameObject* other) {
    if (!other || !gameObject_) {
        return;
    }

    auto playBulletImpact = [this]() {
        if (auto transform = GetTransform()) {
            if (auto effectMgr = EffectManagerComponent::GetInstance()) {
                effectMgr->PlayEffect("debris_dust_effect", transform->GetWorldPosition());
            } else {
                auto effectGo = effectManagerObj_.lock();
                if (!effectGo) {
                    if (auto scene = gameObject_->GetScene()) {
                        effectGo = scene->FindGameObject("EffectManager");
                        effectManagerObj_ = effectGo;
                    }
                }
                if (effectGo) {
                    if (auto effectMgrFallback = effectGo->GetComponent<EffectManagerComponent>()) {
                        effectMgrFallback->PlayEffect("debris_dust_effect", transform->GetWorldPosition());
                    }
                }
            }
        }
    };

    // 1. 【AAA基準: シールド迎撃】自機オービットガレキとの接触
    if (auto debris = other->GetComponent<DebrisComponent>()) {
        if (debris->GetState() == DebrisState::Orbiting) {
            playBulletImpact();
            Deactivate();
            return;
        }
    }

    // 2. 【AAA基準: 無敵時プロジェクタイル消費】素通りバグの解消
    if (auto health = other->GetComponent<PlayerHealthComponent>()) {
        if (!health->IsInvincible()) {
            health->TakeDamage(damage_);
        }
        // 無敵時間中であっても弾は機体表面で着弾・消滅（幽霊素通りを防止）
        playBulletImpact();
        Deactivate();
        return;
    }

    // 3. 【AAA基準: 環境遮蔽】壁や柱（Environment）に着弾消滅
    if (auto collider = other->GetComponent<ColliderComponent>()) {
        if (auto engine = GetEngine()) {
            if (auto cm = engine->GetCollisionManager()) {
                uint32_t envMask = cm->GetLayerMask("Environment");
                if ((collider->layer_ & envMask) != 0) {
                    playBulletImpact();
                    Deactivate();
                    return;
                }
            }
        }
    }
}
