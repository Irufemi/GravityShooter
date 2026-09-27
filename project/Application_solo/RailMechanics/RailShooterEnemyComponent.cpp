#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
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
#include "Renderer/Pipeline/PSOManager.h"
#include "RHI/DirectX12/DirectXCommon.h"
#include <cmath>
#include <algorithm>

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
    state_ = EnemyAIState::Approach;
    stateTimer_ = 0.0f;
    shootTimer_ = 0.6f;
    hoverTimer_ = 0.0f;
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
    if (auto collider = gameObject_->GetComponent<SphereColliderComponent>()) {
        if (auto engine = GetEngine()) {
            if (auto cm = engine->GetCollisionManager()) {
                collider->layer_ = cm->GetLayerMask("Enemy");
                collider->mask_ = cm->GetLayerMask("Player") | cm->GetLayerMask("Debris_Player");
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

    switch (state_) {
    case EnemyAIState::Approach: {
        // 初期距離オフセットから目標交戦距離 (targetDistance_) へスムーズにレール上を接近
        float diffDist = targetDistance_ - currentDistanceOffset_;
        if (std::abs(diffDist) > 0.5f) {
            float moveDir = (diffDist > 0.0f) ? 1.0f : -1.0f;
            currentDistanceOffset_ += moveDir * speed_ * 1.5f * dt;
            if ((moveDir > 0.0f && currentDistanceOffset_ > targetDistance_) ||
                (moveDir < 0.0f && currentDistanceOffset_ < targetDistance_)) {
                currentDistanceOffset_ = targetDistance_;
            }
        } else {
            currentDistanceOffset_ = targetDistance_;
        }

        currentLocalOffset_ = baseFormationOffset_;

        stateTimer_ += dt;
        if (std::abs(currentDistanceOffset_ - targetDistance_) < 2.0f || stateTimer_ >= 3.0f) {
            if (behaviorType_ == static_cast<int>(EnemyBehaviorType::DiveBomber)) {
                state_ = EnemyAIState::Dive;
            } else {
                state_ = EnemyAIState::Combat;
            }
            stateTimer_ = 0.0f;
            shootTimer_ = (behaviorType_ == static_cast<int>(EnemyBehaviorType::PredictiveSniper))
                              ? (std::max)(sniperTelegraphDuration_, 1.2f)
                              : 0.6f; // 初弾タイマー
        }
        break;
    }
    case EnemyAIState::Combat: {
        // 自機と等速で前方一定距離を完全維持
        currentDistanceOffset_ = targetDistance_;

        // レール局所断面での浮遊運動（サイン・コサイン波）
        hoverTimer_ += dt;
        currentLocalOffset_.x = baseFormationOffset_.x + std::sin(hoverTimer_ * 2.5f) * 4.0f;
        currentLocalOffset_.y = baseFormationOffset_.y + std::cos(hoverTimer_ * 2.0f) * 2.5f;

        // 射撃および予兆（Telegraphing）処理
        shootTimer_ -= dt;

        if (behaviorType_ == static_cast<int>(EnemyBehaviorType::PredictiveSniper)) {
            // スナイパー：予兆期間中の射線更新およびロック判定
            if (shootTimer_ <= sniperTelegraphDuration_ && shootTimer_ > 0.0f) {
                if (shootTimer_ > sniperLockLeadTime_) {
                    // [追従フェーズ] プレイヤーの未来位置をリアルタイム計算して射線を追従
                    isAimLocked_ = false;
                    Irufemi::Vector3 myPos = transform->GetWorldPosition();
                    float dx = currentPlayerPos.x - myPos.x;
                    float dy = currentPlayerPos.y - myPos.y;
                    float dz = currentPlayerPos.z - myPos.z;
                    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                    float travelTime = dist / (std::max)(bulletSpeed_, 1.0f);
                    travelTime = std::clamp(travelTime, 0.0f, 1.2f); // 過剰な未来予測の暴走を防止

                    lockedTargetPos_ = {currentPlayerPos.x + playerVelocity_.x * travelTime,
                                        currentPlayerPos.y + playerVelocity_.y * travelTime,
                                        currentPlayerPos.z + playerVelocity_.z * travelTime};

                    Irufemi::Vector3 aimDiff = {lockedTargetPos_.x - myPos.x, lockedTargetPos_.y - myPos.y,
                                                lockedTargetPos_.z - myPos.z};
                    float aimLen = std::sqrt(aimDiff.x * aimDiff.x + aimDiff.y * aimDiff.y + aimDiff.z * aimDiff.z);
                    if (aimLen > 0.0001f) {
                        lockedAimDir_ = {aimDiff.x / aimLen, aimDiff.y / aimLen, aimDiff.z / aimLen};
                    } else {
                        lockedAimDir_ = {0.0f, 0.0f, -1.0f};
                    }
                } else {
                    // [ロック固定フェーズ] 射線固定（追従停止。自機が動いても空間に射線が固定される）
                    isAimLocked_ = true;
                }
            }

            if (shootTimer_ <= 0.0f) {
                // 固定された射線ベクトルへ高威力の偏差弾を発射
                Irufemi::Vector3 myPos = transform->GetWorldPosition();
                if (!bulletManager_) {
                    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
                    if (scene) {
                        bulletManager_ = EnemyBulletManagerComponent::GetOrCreate(scene);
                    }
                }
                if (bulletManager_) {
                    bulletManager_->FireBullet(myPos, lockedAimDir_, bulletSpeed_, 15, bulletScale_);
                }
                shootTimer_ = shootInterval_;
                ResetTelegraph();
            }
        } else {
            // 通常機：自機狙い射撃
            if (shootTimer_ <= 0.0f) {
                if (playerObj && playerObj->GetTransform()) {
                    ShootAtPlayer(currentPlayerPos);
                }
                shootTimer_ = shootInterval_;
            }
        }

        // 一定時間経過で離脱フェーズへ移行
        stateTimer_ += dt;
        if (stateTimer_ >= combatDuration_) {
            state_ = EnemyAIState::Disengage;
            stateTimer_ = 0.0f;
        }
        break;
    }
    case EnemyAIState::Dive: {
        // 特攻機（DiveBomber）：自機前方から急加速して体当たり自爆コースへ突撃
        currentDistanceOffset_ -= speed_ * 1.8f * dt;

        // 突撃しながら自機の正面ラインへ向かって急激に収束
        float tLerp = std::clamp(dt * 2.5f, 0.0f, 1.0f);
        currentLocalOffset_.x = std::lerp(currentLocalOffset_.x, 0.0f, tLerp);
        currentLocalOffset_.y = std::lerp(currentLocalOffset_.y, 0.0f, tLerp);

        // 鋭いコルクスクリュー回転（ロール角加算）
        diveRollAngle_ += dt * 14.0f;

        // 自機後方に完全に抜けたら消滅
        if (currentDistanceOffset_ < -30.0f) {
            NotifyDespawn(DespawnReason::OutOfBounds);
            return;
        }
        break;
    }
    case EnemyAIState::Disengage: {
        // 相対同期を解除し、自機の脇をすり抜けて後方へ急加速
        currentDistanceOffset_ -= speed_ * 2.5f * dt;

        // 自機後方に完全に抜けたら消滅（画面外へ抜けるまで安全に生存）
        if (currentDistanceOffset_ < -30.0f) {
            NotifyDespawn(DespawnReason::OutOfBounds);
            return;
        }
        break;
    }
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
    float currentRoll = (state_ == EnemyAIState::Dive) ? diveRollAngle_ : 0.0f;
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
    if (!isActive_ || !IsAlive() || state_ != EnemyAIState::Combat) {
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
    float dx = playerPos.x - myPos.x;
    float dy = playerPos.y - myPos.y;
    float dz = playerPos.z - myPos.z;
    float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
    float travelTime = dist / (std::max)(bulletSpeed_, 1.0f);
    travelTime = std::clamp(travelTime, 0.0f, 1.2f); // 過剰な未来予測の暴走を防止

    // 自機の未来予測座標
    Irufemi::Vector3 predictedTarget = {playerPos.x + playerVel.x * travelTime, playerPos.y + playerVel.y * travelTime,
                                        playerPos.z + playerVel.z * travelTime};

    Irufemi::Vector3 dir = {predictedTarget.x - myPos.x, predictedTarget.y - myPos.y, predictedTarget.z - myPos.z};
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

    // 1. 投擲されたガレキとの接触時
    if (auto debris = other->GetComponent<DebrisComponent>()) {
        if (debris->GetState() == DebrisState::Thrown) {
            TakeDamage(debris->GetEnemyDamage());
            return;
        }
    }

    // 2. プレイヤー本体との接触時（体当たり）
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
