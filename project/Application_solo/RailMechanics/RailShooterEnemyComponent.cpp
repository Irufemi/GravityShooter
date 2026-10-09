#include "RailMechanics/RailShooterEnemyComponent.h"
#include "RailMechanics/State/RailShooterEnemyStateApproach.h"
#include "RailMechanics/Strategy/EnemyAttackStrategyNormal.h"
#include "RailMechanics/Strategy/EnemyAttackStrategyPredictiveSniper.h"
#include "RailMechanics/Strategy/EnemyAttackStrategyDiveBomb.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Framework/Component/Utility/SplineComponent.h"
#include "RailMechanics/SplineFollowerComponent.h"
#include "Player/TargetableComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Combat/EnemyBulletComponent.h"
#include "Combat/EnemyBulletManagerComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Framework/Scene/BaseScene.h"
#include "Physics/CollisionManager.h"
#include "Core/Math/MathFunction.h"
#include "Environment/DebrisComponent.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include <cmath>
#include <algorithm>

RailShooterEnemyComponent::RailShooterEnemyComponent() = default;
RailShooterEnemyComponent::~RailShooterEnemyComponent() = default;

void RailShooterEnemyComponent::OnRegisterProperties() {
    RegisterProperty("BehaviorType", &behaviorType_);
    RegisterProperty("SpawnProgress", &spawnProgress_);
    RegisterProperty("Speed", &speed_);
    RegisterProperty("HP", &hp_);
    RegisterProperty("CombatDuration", &combatDuration_);
    RegisterProperty("ShootInterval", &shootInterval_);
    RegisterProperty("TargetDistance", &targetDistance_);
    RegisterProperty("BodyDamage", &bodyDamage_);
    RegisterProperty("BulletScale", &bulletScale_);
    RegisterProperty("BulletSpeed", &bulletSpeed_);
    RegisterProperty("CurrentDistanceOffset", &currentDistanceOffset_);
    RegisterProperty("BaseFormationOffsetX", &baseFormationOffset_.x);
    RegisterProperty("BaseFormationOffsetY", &baseFormationOffset_.y);
    RegisterProperty("SniperTelegraphDuration", &sniperTelegraphDuration_);
    RegisterProperty("SniperLockLeadTime", &sniperLockLeadTime_);
    RegisterProperty("SniperLaserRadius", &sniperLaserRadius_);
    RegisterProperty("SniperLaserLength", &sniperLaserLength_);
    RegisterProperty("SniperLaserColor", &sniperLaserColor_);
}

void RailShooterEnemyComponent::Initialize() {
    if (!gameObject_) {
        return;
    }
    hp_ = 100;
    isActive_ = true;
    SetBehaviorType(static_cast<EnemyBehaviorType>(behaviorType_));
    ChangeState(std::make_unique<RailShooterEnemyStateApproach>());
    diveRollAngle_ = 0.0f;
    hasLastPlayerPos_ = false;
    playerFollower_ = nullptr;
    cachedSpline_ = nullptr;
    baseFormationOffset_ = {0.0f, 0.0f};
    currentLocalOffset_ = {0.0f, 0.0f};
    currentDistanceOffset_ = targetDistance_ + 20.0f;
    isAimLocked_ = false;
    lockedAimDir_ = {0.0f, 0.0f, -1.0f};
    lockedTargetPos_ = {0.0f, 0.0f, 0.0f};

    auto targetable = gameObject_->GetComponent<TargetableComponent>();
    if (!targetable) {
        targetable = gameObject_->AddComponent<TargetableComponent>().get();
        targetable->Initialize();
    }
    if (targetable) {
        targetable->SetTargetablePredicate([this]() { return IsAlive(); });
    }

    // コライダーのサイズ・形状はプレハブ（アセット）側を100%尊重し、コードによる勝手な追加・上書きを行わない
    if (auto collider = gameObject_->GetComponent<ColliderComponent>()) {
        if (auto engine = GetEngine()) {
            if (auto cm = engine->GetCollisionManager()) {
                collider->SetLayer(cm->GetLayerMask("Enemy"));
                collider->SetMask(cm->GetLayerMask("Player") | cm->GetLayerMask("Debris_Player"));
            }
        }
    }
}

void RailShooterEnemyComponent::SetRailTrackingParams(SplineComponent* spline, SplineFollowerComponent* follower,
                                                      float initialDistOffset, float targetDistOffset,
                                                      const Irufemi::Vector2& formationOffset) {
    cachedSpline_ = spline;
    playerFollower_ = follower;
    currentDistanceOffset_ = initialDistOffset;
    targetDistance_ = targetDistOffset;
    baseFormationOffset_ = formationOffset;
    currentLocalOffset_ = formationOffset;
}

void RailShooterEnemyComponent::Start() {
    if (auto scene = gameObject_ ? gameObject_->GetScene() : nullptr) {
        bulletManager_ = EnemyBulletManagerComponent::GetOrCreate(scene);
    }
}

GameObject* RailShooterEnemyComponent::GetPlayerObject() {
    if (!gameObject_) {
        return nullptr;
    }
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return nullptr;
    }
    auto player = scene->FindGameObject("Player");
    if (!player) {
        player = scene->FindGameObject("PlayerCart");
    }
    return player.get();
}

void RailShooterEnemyComponent::Update() {
    if (!gameObject_ || !isActive_) {
        return;
    }

    float dt = GetEngine() ? GetEngine()->GetGameDeltaTime() : 0.0f;
    if (dt <= 0.0f) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    // 被弾ヒットフラッシュタイマーの更新 (Juice)
    if (hitFlashTimer_ > 0.0f) {
        hitFlashTimer_ -= dt;
        if (hitFlashTimer_ <= 0.0f) {
            hitFlashTimer_ = 0.0f;
            if (auto maskComp = gameObject_->GetComponent<EffectMaskComponent>()) {
                auto params = maskComp->GetCustomParams();
                params.color1 = originalOutlineColor_;
                maskComp->SetCustomParams(params);
            }
        }
    }

    // スプライン追従情報が未解決の場合はプレイヤーから自動解決
    if (!playerFollower_ || !cachedSpline_) {
        auto playerObj = GetPlayerObject();
        if (playerObj) {
            if (!playerFollower_) {
                playerFollower_ = playerObj->GetComponent<SplineFollowerComponent>();
            }
            if (playerFollower_ && !cachedSpline_) {
                cachedSpline_ = playerFollower_->GetCachedPath();
            }
        }
    }

    // フォールバック（スプラインが無い場合のみ直線前進）
    if (!playerFollower_ || !cachedSpline_) {
        Irufemi::Vector3 myPos = transform->GetWorldPosition();
        myPos.z -= speed_ * dt;
        transform->SetWorldPosition(myPos);
        return;
    }

    // プレイヤーの実効移動速度を計測
    Irufemi::Vector3 currentPlayerPos = {0.0f, 0.0f, 0.0f};
    auto playerObj = GetPlayerObject();
    if (playerObj && playerObj->GetTransform()) {
        currentPlayerPos = playerObj->GetTransform()->GetWorldPosition();
        if (hasLastPlayerPos_ && dt > 0.0001f) {
            playerVelocity_ = {(currentPlayerPos.x - lastPlayerPos_.x) / dt,
                               (currentPlayerPos.y - lastPlayerPos_.y) / dt,
                               (currentPlayerPos.z - lastPlayerPos_.z) / dt};
        } else {
            playerVelocity_ = {0.0f, 0.0f, 0.0f};
        }
        lastPlayerPos_ = currentPlayerPos;
        hasLastPlayerPos_ = true;
    }

    float playerDist = playerFollower_->GetCurrentDistance();

    if (currentState_) {
        currentState_->Update(this, dt);
    }

    // レールスプライン上の位置と局所直交基底（Frenet-Serret）を計算
    float enemyRailDist = playerDist + currentDistanceOffset_;
    float totalLength = cachedSpline_->GetTotalLength();
    if (enemyRailDist < 0.0f) {
        enemyRailDist = 0.0f;
    }
    if (totalLength > 0.0f && enemyRailDist > totalLength) {
        enemyRailDist = totalLength;
    }

    Irufemi::Vector3 railCenter = cachedSpline_->GetPointAtDistance(enemyRailDist);
    Irufemi::Vector3 tangent = cachedSpline_->GetTangentAtDistance(enemyRailDist);

    // 局所基底の算出（Right, Up）
    Irufemi::Vector3 upWorld = {0.0f, 1.0f, 0.0f};
    Irufemi::Vector3 right = Irufemi::Math::Cross(upWorld, tangent);
    float lenR = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
    if (lenR > 0.0001f) {
        right.x /= lenR;
        right.y /= lenR;
        right.z /= lenR;
    } else {
        right = {1.0f, 0.0f, 0.0f};
    }
    Irufemi::Vector3 up = Irufemi::Math::Normalize(Irufemi::Math::Cross(tangent, right));

    // 最終ワールド座標の決定
    Irufemi::Vector3 finalPos = railCenter;
    finalPos.x += right.x * currentLocalOffset_.x + up.x * currentLocalOffset_.y;
    finalPos.y += right.y * currentLocalOffset_.x + up.y * currentLocalOffset_.y;
    finalPos.z += right.z * currentLocalOffset_.x + up.z * currentLocalOffset_.y;
    transform->SetWorldPosition(finalPos);

    // 自機と対面（-tangent）する姿勢制御（Dive中はロール角を加算）
    Irufemi::Vector3 lookDir = {-tangent.x, -tangent.y, -tangent.z};
    float yaw = std::atan2(lookDir.x, lookDir.z);
    float pitch = std::asin(std::clamp(-lookDir.y, -1.0f, 1.0f));
    float currentRoll = (GetStateType() == EnemyAIState::Dive) ? diveRollAngle_ : 0.0f;
    transform->SetWorldRotation(Irufemi::Vector3{pitch, yaw, currentRoll});
}

void RailShooterEnemyComponent::EnsureTelegraphResources() {
    if (telegraphCylinder_) {
        return;
    }
    auto engine = GetEngine();
    if (!engine) {
        return;
    }

    telegraphCylinder_ = std::make_unique<Primitive3DObject>();
    telegraphCylinder_->Initialize(Irufemi::PrimitiveType::Cylinder);
    telegraphCylinder_->SetColor(sniperLaserColor_);
    telegraphCylinder_->SetCastShadows(false);
    telegraphCylinder_->SetCullingEnabled(false);
    telegraphCylinder_->SetIsTransparent(true);

    auto dxCommon = engine->GetDirectXCommon();
    if (dxCommon) {
        aoeParamsBuffer_.Initialize(dxCommon);
        aoeParamsData_ = AOEParams();
        aoeParamsData_.shapeType = 2; // Cylinder用
        aoeParamsData_.warningRatio = 0.0f;
        aoeParamsBuffer_.UpdateAll(aoeParamsData_);
    }
}

void RailShooterEnemyComponent::ResetTelegraph() {
    isAimLocked_ = false;
    lockedAimDir_ = {0.0f, 0.0f, -1.0f};
    lockedTargetPos_ = {0.0f, 0.0f, 0.0f};
}

void RailShooterEnemyComponent::OnDisable() {
    ResetTelegraph();
}

void RailShooterEnemyComponent::Draw() {
    if (behaviorType_ != static_cast<int>(EnemyBehaviorType::PredictiveSniper)) {
        return;
    }
    if (!isActive_ || !IsAlive() || GetStateType() != EnemyAIState::Combat) {
        return;
    }
    if (shootTimer_ > sniperTelegraphDuration_ || shootTimer_ <= 0.0f) {
        return;
    }

    EnsureTelegraphResources();
    if (!telegraphCylinder_) {
        return;
    }

    auto engine = GetEngine();
    if (!engine || !engine->GetDirectXCommon()) {
        return;
    }

    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    Irufemi::Vector3 myPos = transform->GetWorldPosition();
    float warningRatio = std::clamp(1.0f - (shootTimer_ / sniperTelegraphDuration_), 0.0f, 1.0f);

    // ロック固定中は赤と白の超高速パルス点滅で強烈に発射警告
    if (isAimLocked_) {
        float pulse = std::sin(shootTimer_ * 55.0f);
        Irufemi::Vector4 flashColor =
            (pulse > 0.0f) ? Irufemi::Vector4{1.0f, 0.15f, 0.15f, 0.95f} : Irufemi::Vector4{1.0f, 0.95f, 0.95f, 1.0f};
        telegraphCylinder_->SetColor(flashColor);
        aoeParamsData_.warningRatio = 1.0f;
    } else {
        telegraphCylinder_->SetColor(sniperLaserColor_);
        aoeParamsData_.warningRatio = warningRatio;
    }

    aoeParamsData_.shapeType = 2; // Cylinder
    uint32_t frameIndex = engine->GetDirectXCommon()->GetCurrentBackBufferIndex();
    aoeParamsBuffer_.Update(aoeParamsData_, frameIndex);

    // シリンダーの姿勢・サイズ設定（高さ方向: Y軸(0, 1, 0)基準）
    Irufemi::Matrix4x4 rotMat = Irufemi::Math::DirectionToDirection({0.0f, 1.0f, 0.0f}, lockedAimDir_);
    Irufemi::Vector3 rotate = Irufemi::Math::ExtractEulerFromMatrix(rotMat);
    Irufemi::Vector3 center = myPos + lockedAimDir_ * (sniperLaserLength_ * 0.5f);

    telegraphCylinder_->SetPosition(center);
    telegraphCylinder_->SetRotate(rotate);
    telegraphCylinder_->SetScale({sniperLaserRadius_, sniperLaserLength_, sniperLaserRadius_});
    telegraphCylinder_->Update();

    telegraphCylinder_->SetCustomPSO("AOEWarning", Irufemi::BlendMode::kBlendModeAdd, PSOManager::DepthWrite::Disable,
                                     PSOManager::CullMode::None);
    telegraphCylinder_->SetCustomCBVAddress(aoeParamsBuffer_.GetGPUVirtualAddress(frameIndex));
    telegraphCylinder_->Draw();
}

void RailShooterEnemyComponent::ShootPredictiveAtPlayer(const Irufemi::Vector3& playerPos,
                                                        const Irufemi::Vector3& playerVel) {
    if (!gameObject_) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }
    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    Irufemi::Vector3 myPos = transform->GetWorldPosition();
    Irufemi::Vector3 dir = lockedAimDir_;

    // 射線ロック固定中でない場合はプレイヤーの移動ベクトルから未来予測射線を算出
    if (!isAimLocked_) {
        float dx = playerPos.x - myPos.x;
        float dy = playerPos.y - myPos.y;
        float dz = playerPos.z - myPos.z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        float travelTime = dist / (std::max)(bulletSpeed_, 1.0f);
        travelTime = std::clamp(travelTime, 0.0f, 1.2f); // 過剰な未来予測の暴走を防止

        // 自機の未来予測座標
        Irufemi::Vector3 predictedTarget = {playerPos.x + playerVel.x * travelTime,
                                            playerPos.y + playerVel.y * travelTime,
                                            playerPos.z + playerVel.z * travelTime};

        Irufemi::Vector3 diff = {predictedTarget.x - myPos.x, predictedTarget.y - myPos.y, predictedTarget.z - myPos.z};
        float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
        if (len > 0.0001f) {
            dir = {diff.x / len, diff.y / len, diff.z / len};
        } else {
            dir = {0.0f, 0.0f, -1.0f};
        }
    }

    if (!bulletManager_) {
        bulletManager_ = EnemyBulletManagerComponent::GetOrCreate(scene);
    }
    if (bulletManager_) {
        // スナイパーは高威力(15)の高速弾
        bulletManager_->FireBullet(myPos, dir, bulletSpeed_, 15, bulletScale_);
    }
}

void RailShooterEnemyComponent::ShootAtPlayer(const Irufemi::Vector3& playerPos) {
    if (!gameObject_) {
        return;
    }
    auto scene = gameObject_->GetScene();
    if (!scene) {
        return;
    }
    auto transform = GetTransform();
    if (!transform) {
        return;
    }

    Irufemi::Vector3 myPos = transform->GetWorldPosition();
    Irufemi::Vector3 dir = {playerPos.x - myPos.x, playerPos.y - myPos.y, playerPos.z - myPos.z};
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.0001f) {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    } else {
        dir = {0.0f, 0.0f, -1.0f};
    }

    if (!bulletManager_) {
        bulletManager_ = EnemyBulletManagerComponent::GetOrCreate(scene);
    }
    if (bulletManager_) {
        bulletManager_->FireBullet(myPos, dir, bulletSpeed_, 10, bulletScale_);
    }
}

void RailShooterEnemyComponent::OnCollisionEnter(GameObject* other) {
    if (!other || !gameObject_) {
        return;
    }

    // 同一フレーム内の重複ダメージ防止:
    // 投擲ガレキからの被弾は DebrisComponent 側の IDamageable::TakeDamage にて一元処理されるため、
    // ここでの TakeDamage 呼び出しは行わない（二重ダメージの解消）

    // プレイヤー本体との接触時（体当たり自爆）
    if (auto health = other->GetComponent<PlayerHealthComponent>()) {
        if (!health->IsInvincible()) {
            health->TakeDamage(bodyDamage_);
        }
        // 体当たり後は敵自身も自爆・撃破
        hp_ = 0;
        NotifyDespawn(DespawnReason::CollisionSuicide);
    }
}

void RailShooterEnemyComponent::TakeDamage(float damage) {
    if (damage <= 0.0f) {
        return;
    }
    int intDamage = static_cast<int>(std::round(damage));
    if (intDamage < 1) {
        intDamage = 1;
    }
    TakeDamage(intDamage);
}

void RailShooterEnemyComponent::TakeDamage(int damage) {
    if (!IsAlive()) {
        return;
    }
    if (!gameObject_ || gameObject_->IsDestroyed()) {
        return;
    }

    hp_ -= damage;

    // 被弾時の白熱ヒットフラッシュ演出 (Juice)
    if (auto maskComp = gameObject_->GetComponent<EffectMaskComponent>()) {
        if (!hasCachedOriginalOutline_) {
            originalOutlineColor_ = maskComp->GetCustomParams().color1;
            hasCachedOriginalOutline_ = true;
        }
        auto params = maskComp->GetCustomParams();
        params.color1 = Irufemi::Vector4{2.5f, 2.5f, 2.5f, 1.0f}; // 白熱閃光
        maskComp->SetCustomParams(params);
        hitFlashTimer_ = 0.08f; // 約5フレーム閃光
    }

    if (hp_ <= 0) {
        hp_ = 0;
        NotifyDespawn(DespawnReason::KilledByPlayer);
    }
}

void RailShooterEnemyComponent::NotifyDespawn(DespawnReason reason) {
    isActive_ = false;
    ResetTelegraph();
    if (onDespawnListener_) {
        onDespawnListener_(gameObject_, reason);
    } else if (onDeathCallback_ &&
               (reason == DespawnReason::KilledByPlayer || reason == DespawnReason::CollisionSuicide)) {
        onDeathCallback_(gameObject_);
    } else if (gameObject_) {
        gameObject_->SetIsActive(false);
        gameObject_->Destroy();
    }
}

void RailShooterEnemyComponent::ChangeState(std::unique_ptr<IRailShooterEnemyState> newState) {
    if (currentState_) {
        currentState_->Exit(this);
    }
    currentState_ = std::move(newState);
    if (currentState_) {
        currentState_->Enter(this);
    }
}

EnemyAIState RailShooterEnemyComponent::GetStateType() const {
    if (currentState_) {
        return currentState_->GetStateType();
    }
    return EnemyAIState::Approach;
}

void RailShooterEnemyComponent::SetBehaviorType(EnemyBehaviorType type) {
    behaviorType_ = static_cast<int>(type);
    switch (type) {
    case EnemyBehaviorType::StandardGunner:
        SetAttackStrategy(std::make_unique<EnemyAttackStrategyNormal>());
        break;
    case EnemyBehaviorType::PredictiveSniper:
        SetAttackStrategy(std::make_unique<EnemyAttackStrategyPredictiveSniper>());
        break;
    case EnemyBehaviorType::DiveBomber:
        SetAttackStrategy(std::make_unique<EnemyAttackStrategyDiveBomb>());
        break;
    default:
        SetAttackStrategy(std::make_unique<EnemyAttackStrategyNormal>());
        break;
    }
}

void RailShooterEnemyComponent::SetAttackStrategy(std::unique_ptr<IEnemyAttackStrategy> strategy) {
    attackStrategy_ = std::move(strategy);
}

