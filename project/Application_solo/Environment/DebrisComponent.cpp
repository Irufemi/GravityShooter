#include "Environment/DebrisComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Combat/IDamageable.h"
#include "Combat/Boss/BossComponent.h"
#include "Player/TargetableComponent.h"
#include "Environment/DebrisManagerComponent.h"
#include "Effects/EffectManagerComponent.h"
#include "Core/Math/Random/Random.h"
#include "Core/Math/MathFunction.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Framework/Component/Camera/CameraShakeComponent.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include <cmath>
#include <windows.h>
#include <iostream>
#include "Core/Utility/Log.h"
#include "Physics/CollisionManager.h"

float DebrisComponent::GetPullSpeed() const {
    return manager_ ? manager_->GetDebrisPullSpeed() : 10.0f;
}
float DebrisComponent::GetThrowSpeed() const {
    return manager_ ? manager_->GetDebrisThrowSpeed() : 50.0f;
}
float DebrisComponent::GetOrbitSpeed() const {
    return manager_ ? manager_->GetDebrisOrbitSpeed() : 2.0f;
}
float DebrisComponent::GetBossDamage() const {
    return manager_ ? manager_->GetDebrisDamage() : 10.0f;
}
float DebrisComponent::GetEnemyDamage() const {
    return manager_ ? manager_->GetDebrisEnemyDamage() : 100.0f;
}
float DebrisComponent::GetCameraShakeIntensity() const {
    return manager_ ? manager_->GetCameraShakeIntensity() : 0.5f;
}
int DebrisComponent::GetCameraShakeDurationFrames() const {
    return manager_ ? manager_->GetCameraShakeDurationFrames() : 10;
}
Irufemi::Vector4 DebrisComponent::GetPlayerAuraColor() const {
    return manager_ ? manager_->GetPlayerAuraColor() : Irufemi::Vector4{0.0f, 0.8f, 1.0f, 0.4f};
}
Irufemi::Vector4 DebrisComponent::GetBossAuraColor() const {
    return manager_ ? manager_->GetBossAuraColor() : Irufemi::Vector4{0.8f, 0.0f, 0.6f, 0.4f};
}
float DebrisComponent::GetCatchDistanceSq() const {
    return manager_ ? manager_->GetCatchDistanceSq() : 2.0f;
}
float DebrisComponent::GetBossShieldRadius() const {
    return manager_ ? manager_->GetBossShieldRadius() : 8.0f;
}
float DebrisComponent::GetPullYOffset() const {
    return manager_ ? manager_->GetDebrisPullYOffset() : 2.0f;
}

void DebrisComponent::OnRegisterProperties() {
    RegisterProperty("Hit Effect Key", &hitEffectKey_);
    RegisterProperty("Explosion Model Path", &explosionModelPath_);
}

void DebrisComponent::Initialize() {
    // 必要なコンポーネントのキャッシュや初期化のみ行う
}

void DebrisComponent::OnEnable() {
    state_ = DebrisState::Idle;
    targetObject_.reset();
    idleTimeY_ = Irufemi::Random::GeneratorFloat(0.0f, 100.0f); // ランダムな位相で開始

    if (gameObject_) {
        if (auto targetable = gameObject_->GetComponent<TargetableComponent>()) {
            targetable->SetTargetType(TargetType::BossShield);
            targetable->SetTargetablePredicate([this]() { return state_ == DebrisState::BossOrbiting; });
        }
    }

    if (auto transform = GetTransform()) {
        baseIdleY_ = transform->GetPosition().y;
    }
}

void DebrisComponent::OnDisable() {
    if (manager_) {
        manager_->UnregisterDebris(this, state_);
    }
}

void DebrisComponent::DestroyAsShield() {
    if (state_ != DebrisState::BossOrbiting) {
        return;
    }

    // Bossからシールドを解除する
    if (auto bossTarget = targetObject_.lock()) {
        if (auto bossTargetComp = bossTarget->GetComponent<BossComponent>()) {
            if (gameObject_) {
                bossTargetComp->RemoveShield(gameObject_->shared_from_this());
            }
        }
    }

    // シールドを消滅させる
    if (manager_ && gameObject_) {
        manager_->MarkForRelease(gameObject_->shared_from_this());
        if (virtualId_ >= 0) {
            manager_->MarkForDestroy(virtualId_, variationIndex_);
        }
    } else if (gameObject_) {
        gameObject_->SetIsActive(false);
    }
}

void DebrisComponent::OnCollisionEnter(GameObject* otherObj) {
    if (state_ != DebrisState::Thrown) {
        return;
    }
    if (!otherObj) {
        return;
    }

    // 1. オーナー（発射元自機）との相互除外 (Unreal Engine の IgnoreActor / Instigator パターン)
    if (auto owner = ownerObject_.lock()) {
        if (otherObj == owner.get()) {
            return;
        }
    }

    // 2. 自機レイヤー（Player）に属するオブジェクト（自機パーツ・コライダー）との衝突も完全に除外
    if (auto engine = GetEngine()) {
        if (auto cm = engine->GetCollisionManager()) {
            if (auto otherCol = otherObj->GetComponent<ColliderComponent>()) {
                uint32_t playerMask = cm->GetLayerMask("Player");
                if ((otherCol->GetLayer() & playerMask) != 0) {
                    return;
                }
            }
        }
    }

    bool hit = false;
    if (auto debrisComp = otherObj->GetComponent<DebrisComponent>()) {
        if (debrisComp->GetState() == DebrisState::BossOrbiting) {
            // 有効な衝突対象であることが確定した瞬間のみ、原子的（Atomic）に権限を消費
            if (!ConsumeHitAuthority()) {
                return;
            }
            debrisComp->DestroyAsShield();
            hit = true;
        }
    } else if (auto damageable = otherObj->GetComponentByInterface<IDamageable>()) {
        // 有効なダメージ対象であることが確定した瞬間のみ権限を消費
        if (!ConsumeHitAuthority()) {
            return;
        }
        using DamageGetter = float (DebrisComponent::*)() const;
        static const std::unordered_map<DamageableType, DamageGetter> kDamageGetters = {
            { DamageableType::Boss,  &DebrisComponent::GetBossDamage },
            { DamageableType::Enemy, &DebrisComponent::GetEnemyDamage },
        };
        float damage = 1.0f;
        if (auto it = kDamageGetters.find(damageable->GetDamageableType()); it != kDamageGetters.end()) {
            damage = (this->*(it->second))();
        }
        damageable->TakeDamage(damage);
        hit = true;
    } else if (auto collider = otherObj ? otherObj->GetComponent<ColliderComponent>() : nullptr) {
        auto cm = GetEngine() ? GetEngine()->GetCollisionManager() : nullptr;
        // 建造物（Environmentレイヤー）との衝突検知
        // 衝突した場合は破砕エフェクトを再生し、プールへ返却（回収）する
        if (cm) {
            uint32_t envMask = cm->GetLayerMask("Environment");
            if ((collider->GetLayer() & envMask) != 0) {
                if (!ConsumeHitAuthority()) {
                    return;
                }
                hit = true;
            }
        }
    }

    if (hit) {
        // 直撃の重厚感を演出するヒットストップ（約2〜3フレーム）
        if (auto engine = GetEngine()) {
            engine->TriggerHitStop(0.04f);
        }

        if (auto t = GetTransform()) {
            Irufemi::Vector3 hitPos = t->GetWorldPosition();

            EffectManagerComponent* effectManager = nullptr;
            if (auto go = effectManagerObj_.lock()) {
                effectManager = go->GetComponent<EffectManagerComponent>();
            } else if (gameObject_ && gameObject_->GetScene()) {
                if (auto found = gameObject_->GetScene()->FindGameObject("EffectManager")) {
                    effectManagerObj_ = found;
                    effectManager = found->GetComponent<EffectManagerComponent>();
                }
            }
            if (effectManager) {
                effectManager->PlayEffect(hitEffectKey_, hitPos);
            }

            auto engine = GetEngine();
            if (auto voxelManager = engine ? engine->GetVoxelParticleManager() : nullptr) {
                VoxelEmitter p{};
                p.particleType = 5; // DebrisExplosive
                p.lifeTime = 1.0f;
                p.gravity = 5.0f;
                p.dispersion = 12.0f;
                p.scale = {0.5f, 0.5f, 0.5f};

                Irufemi::Vector4 aura =
                    (state_ == DebrisState::BossOrbiting) ? GetBossAuraColor() : GetPlayerAuraColor();
                Irufemi::Vector4 rockColor = {1.5f, 1.2f, 1.0f, 1.0f};
                p.startColor = {rockColor.x + aura.x * 2.0f, rockColor.y + aura.y * 2.0f, rockColor.z + aura.z * 2.0f,
                                1.0f};
                p.endColor = {0.2f, 0.2f, 0.2f, 1.0f};
                p.dissolveEdgeColor = aura;

                voxelManager->PlayExplosion(explosionModelPath_, hitPos, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, p, {2, 2, 2});
            }
        }
        if (manager_) {
            manager_->MarkForRelease(gameObject_->shared_from_this());
            if (virtualId_ >= 0) {
                manager_->MarkForDestroy(virtualId_, variationIndex_);
            }
        } else {
            gameObject_->SetIsActive(false);
        }
    }
}

void DebrisComponent::UpdateAuraVisuals() {
    if (!gameObject_) {
        return;
    }

    for (auto& child : gameObject_->GetChildren()) {
        if (child && child->GetName() == "DebrisAura") {
            struct AuraProperty {
                bool isActive;
                Irufemi::Vector4 (DebrisComponent::*colorGetter)() const;
            };
            static const std::unordered_map<DebrisState, AuraProperty> kAuraProperties = {
                { DebrisState::Pulled,       { true,  &DebrisComponent::GetPlayerAuraColor } },
                { DebrisState::Orbiting,     { true,  &DebrisComponent::GetPlayerAuraColor } },
                { DebrisState::Thrown,       { true,  &DebrisComponent::GetPlayerAuraColor } },
                { DebrisState::BossOrbiting, { true,  &DebrisComponent::GetBossAuraColor } },
                { DebrisState::Idle,         { false, nullptr } },
            };

            bool isActive = false;
            Irufemi::Vector4 auraColor =
                manager_ ? manager_->GetIdleAuraColor() : Irufemi::Vector4{0.6f, 0.2f, 1.0f, 0.4f};

            if (auto it = kAuraProperties.find(state_); it != kAuraProperties.end()) {
                isActive = it->second.isActive;
                if (it->second.colorGetter) {
                    auraColor = (this->*(it->second.colorGetter))();
                }
            }

            child->SetIsActive(isActive);

            if (auto auraModel = child->GetComponent<PrimitiveRendererComponent>()) {
                if (auto primitive = dynamic_cast<Primitive3DObject*>(auraModel->GetRenderable())) {
                    primitive->SetColor(auraColor);
                }
            }
        }
    }
}

void DebrisComponent::ResetForPool() {
    if (manager_) {
        manager_->UnregisterDebris(this, state_);
    }
    state_ = DebrisState::Idle;
    targetObject_.reset();
    ownerObject_.reset();
    orbitAngle_ = 0.0f;
    currentOrbitRadius_ = orbitRadius_;
    throwDirection_ = {0.0f, 0.0f, 0.0f};
    throwOrigin_ = {0.0f, 0.0f, 0.0f};

    ResetHitAuthority();
    UpdateAuraVisuals();

    if (auto collider = gameObject_ ? gameObject_->GetComponent<ColliderComponent>() : nullptr) {
        auto* cm = GetEngine() ? GetEngine()->GetCollisionManager() : nullptr;
        if (cm) {
            uint32_t neutralLayer = cm->GetLayerMask("Debris_Neutral");
            collider->SetLayer(neutralLayer);
            collider->SetMask(0);
        }
    }
}

void DebrisComponent::SetState(DebrisState newState, bool forceVisualUpdate) {
    if (state_ == newState && !forceVisualUpdate) {
        return;
    }
    if (manager_) {
        manager_->UnregisterDebris(this, state_);
    }
    state_ = newState;
    if (manager_) {
        manager_->RegisterDebris(this, state_);
    }

    UpdateAuraVisuals();

    if (auto collider = gameObject_ ? gameObject_->GetComponent<ColliderComponent>() : nullptr) {
        auto* cm = GetEngine() ? GetEngine()->GetCollisionManager() : nullptr;
        if (cm) {
            uint32_t neutralLayer = cm->GetLayerMask("Debris_Neutral");
            uint32_t playerLayer = cm->GetLayerMask("Debris_Player");
            uint32_t enemyLayer = cm->GetLayerMask("Debris_Enemy");

            uint32_t maskEnemy = cm->GetLayerMask("Enemy");
            uint32_t maskPlayer = cm->GetLayerMask("Player");
            uint32_t maskEnvironment = cm->GetLayerMask("Environment");

            struct StateCollisionConfig {
                uint32_t layer;
                uint32_t mask;
                bool resetAuthority;
            };

            const std::unordered_map<DebrisState, StateCollisionConfig> kStateConfigs = {
                { DebrisState::Idle,         { neutralLayer, 0, false } },
                { DebrisState::Pulled,       { neutralLayer, 0, false } },
                { DebrisState::Orbiting,     { playerLayer,  maskEnemy, false } },
                { DebrisState::Thrown,       { playerLayer,  maskEnemy | maskEnvironment | enemyLayer, true } },
                { DebrisState::BossOrbiting, { enemyLayer,   maskPlayer | playerLayer, false } },
            };

            if (auto it = kStateConfigs.find(state_); it != kStateConfigs.end()) {
                collider->SetLayer(it->second.layer);
                collider->SetMask(it->second.mask);
                if (it->second.resetAuthority) {
                    ResetHitAuthority(); // 投擲開始時に判定権限（Arming）を確実にリセット
                }
            }
        }
    }

    if (state_ == DebrisState::BossOrbiting) {
        bossOrbitAngleX_ = Irufemi::Random::GeneratorFloat(0.0f, Irufemi::Math::PI * 2.0f);
        bossOrbitAngleY_ = Irufemi::Random::GeneratorFloat(0.0f, Irufemi::Math::PI * 2.0f);
        bossOrbitAngleZ_ = Irufemi::Random::GeneratorFloat(0.0f, Irufemi::Math::PI * 2.0f);
        bossOrbitSpeedX_ = Irufemi::Random::GeneratorFloat(-1.2f, 1.2f);
        bossOrbitSpeedY_ = Irufemi::Random::GeneratorFloat(-3.0f, 3.0f);
        bossOrbitSpeedZ_ = Irufemi::Random::GeneratorFloat(-1.2f, 1.2f);
        bossOrbitRadiusOffset_ = Irufemi::Random::GeneratorFloat(-1.0f, 1.0f);
    }

    if (state_ == DebrisState::Thrown && gameObject_) {
        if (auto transform = GetTransform()) {
            throwOrigin_ = transform->GetWorldPosition();
        }
    }
}

std::shared_ptr<Component> DebrisComponent::Clone() {
    auto clone = std::make_shared<DebrisComponent>();
    clone->CopyPropertiesFrom(this);
    clone->state_ = this->state_;
    clone->virtualId_ = this->virtualId_;
    clone->variationIndex_ = this->variationIndex_;
    clone->manager_ = this->manager_;
    clone->targetObject_ = this->targetObject_;
    clone->ownerObject_ = this->ownerObject_;
    return clone;
}

bool DebrisComponent::UpdatePullMovement(const Irufemi::Vector3& targetPos, float pullSpeed, float catchDistSq,
                                         float deltaTime) {
    auto transform = GetTransform();
    if (!transform) {
        return false;
    }

    Irufemi::Vector3 pos = transform->GetWorldPosition();
    float pullYOffset = GetPullYOffset();
    Irufemi::Vector3 diff = {targetPos.x - pos.x, targetPos.y + pullYOffset - pos.y, targetPos.z - pos.z};
    pos.x += diff.x * pullSpeed * deltaTime;
    pos.y += diff.y * pullSpeed * deltaTime;
    pos.z += diff.z * pullSpeed * deltaTime;
    transform->SetWorldPosition(pos);

    float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
    return distSq < catchDistSq;
}

void DebrisComponent::InitializeOrbitTransition(const TransformComponent* targetTransform) {
    auto transform = GetTransform();
    if (!transform || !targetTransform) {
        return;
    }

    // 自機の進行方向から水平な前進・右ベクトルを構築（機体のロール・ピッチ傾きによる振れを遮断）
    Irufemi::Vector3 rawFwd = targetTransform->GetWorldForward();
    Irufemi::Vector3 forward = {rawFwd.x, 0.0f, rawFwd.z};
    float len = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    forward = (len > 0.001f) ? Irufemi::Vector3{forward.x / len, 0.0f, forward.z / len}
                             : Irufemi::Vector3{0.0f, 0.0f, 1.0f};
    Irufemi::Vector3 right = {forward.z, 0.0f, -forward.x}; // 水平右ベクトル

    // 自機から現在ガレキ位置への差分ベクトル
    Irufemi::Vector3 diff = transform->GetWorldPosition() - targetTransform->GetWorldPosition();

    float localX = Irufemi::Math::Dot(diff, right);
    float localZ = Irufemi::Math::Dot(diff, forward);

    // 自機の現在向きに対するローカル位相（角度）を逆算して設定（瞬間的な角度飛び・テレポートを根絶）
    if (std::abs(localX) > 0.001f || std::abs(localZ) > 0.001f) {
        orbitAngle_ = std::atan2(localZ, localX);
        float currentDist = std::sqrt(localX * localX + localZ * localZ);
        currentOrbitRadius_ = (std::max)(currentDist, 0.5f);
    } else {
        currentOrbitRadius_ = orbitRadius_;
    }
}

void DebrisComponent::UpdatePlayerOrbit(const TransformComponent* targetTransform, float orbitSpeed, float deltaTime) {
    auto transform = GetTransform();
    if (!transform || !targetTransform) {
        return;
    }

    // 1. 公転角度の更新
    orbitAngle_ += orbitSpeed * deltaTime;

    // 2. 指数平滑化 (Exponential Smoothing) によるフレームレート非依存の滑らかな半径遷移（ワープ防止）
    float blendFactor = 1.0f - std::exp(-8.0f * deltaTime);
    currentOrbitRadius_ = std::lerp(currentOrbitRadius_, orbitRadius_, blendFactor);

    // 3. 水平安定化空間での軌道オフセット計算 (XZ平面円運動 + Y軸微小リサージュ波)
    Irufemi::Vector3 localOffset = {
        std::cos(orbitAngle_) * currentOrbitRadius_,
        std::sin(orbitAngle_ * 2.0f) * 0.5f + 1.0f,
        std::sin(orbitAngle_) * currentOrbitRadius_
    };

    // 4. 機体のロール・ピッチ（バンキング傾き）による振れを遮断し、水平面（Up = {0, 1, 0}）を安定キープ
    Irufemi::Vector3 rawFwd = targetTransform->GetWorldForward();
    Irufemi::Vector3 forward = {rawFwd.x, 0.0f, rawFwd.z};
    float len = std::sqrt(forward.x * forward.x + forward.z * forward.z);
    forward = (len > 0.001f) ? Irufemi::Vector3{forward.x / len, 0.0f, forward.z / len}
                             : Irufemi::Vector3{0.0f, 0.0f, 1.0f};
    Irufemi::Vector3 right = {forward.z, 0.0f, -forward.x};
    Irufemi::Vector3 up = {0.0f, 1.0f, 0.0f};

    Irufemi::Vector3 worldOffset = right * localOffset.x + up * localOffset.y + forward * localOffset.z;

    Irufemi::Vector3 pos = targetTransform->GetWorldPosition() + worldOffset;
    transform->SetWorldPosition(pos);

    // 5. 自転（Tumbling）回転の付与：公転に合わせてガレキ本体もタンブリング回転
    Irufemi::Vector3 currentRot = transform->GetRotation();
    currentRot.x += orbitSpeed * 1.5f * deltaTime;
    currentRot.y += orbitSpeed * 2.0f * deltaTime;
    transform->SetRotation(currentRot);
}

void DebrisComponent::UpdateBossShieldOrbit(const Irufemi::Vector3& targetPos, float currentRadiusBase,
                                            float shieldRotationSpeed, float deltaTime) {
    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    bossOrbitAngleX_ += bossOrbitSpeedX_ * shieldRotationSpeed * deltaTime;
    bossOrbitAngleY_ += bossOrbitSpeedY_ * shieldRotationSpeed * deltaTime;
    bossOrbitAngleZ_ += bossOrbitSpeedZ_ * shieldRotationSpeed * deltaTime;

    Irufemi::Matrix4x4 rotMatrix =
        Irufemi::Math::MakeRotateXYZMatrix(Irufemi::Vector3{bossOrbitAngleX_, bossOrbitAngleY_, bossOrbitAngleZ_});
    float currentRadius = currentRadiusBase + bossOrbitRadiusOffset_;
    Irufemi::Vector3 baseOffset = {0, 0, currentRadius};
    Irufemi::Vector3 localPos = Irufemi::Math::TransformNormal(baseOffset, rotMatrix);

    Irufemi::Vector3 pos = {targetPos.x + localPos.x, targetPos.y + localPos.y, targetPos.z + localPos.z};
    transform->SetWorldPosition(pos);

    transform->SetRotation({bossOrbitAngleX_ * 2.0f, bossOrbitAngleY_ * 2.0f, bossOrbitAngleZ_ * 2.0f});
}

bool DebrisComponent::UpdateThrownMovement(float throwSpeed, float maxDistSq, float deltaTime,
                                           Irufemi::Vector3& outPos) {
    auto transform = GetTransform();
    if (!transform) {
        return false;
    }

    Irufemi::Vector3 pos = transform->GetWorldPosition();
    pos.x += throwDirection_.x * throwSpeed * deltaTime;
    pos.y += throwDirection_.y * throwSpeed * deltaTime;
    pos.z += throwDirection_.z * throwSpeed * deltaTime;
    transform->SetWorldPosition(pos);
    outPos = pos;

    float dx = pos.x - throwOrigin_.x;
    float dy = pos.y - throwOrigin_.y;
    float dz = pos.z - throwOrigin_.z;
    float distSq = dx * dx + dy * dy + dz * dz;
    return distSq > maxDistSq;
}

void DebrisComponent::SetBossOrbitParams(float angleX, float angleY, float angleZ, float speedX, float speedY,
                                         float speedZ, float radiusOffset) {
    bossOrbitAngleX_ = angleX;
    bossOrbitAngleY_ = angleY;
    bossOrbitAngleZ_ = angleZ;
    bossOrbitSpeedX_ = speedX;
    bossOrbitSpeedY_ = speedY;
    bossOrbitSpeedZ_ = speedZ;
    bossOrbitRadiusOffset_ = radiusOffset;
}
