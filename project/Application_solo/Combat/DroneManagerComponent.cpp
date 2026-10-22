#include "Combat/DroneManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"
#include "Renderer/Object/Batch/ModelBatch.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Framework/Component/TransformComponent.h"
#include "Core/Math/MathFunction.h"
#include "Core/Math/Random/Random.h"
#include "Core/System/IrufemiEngine.h"
#include "Combat/Boss/BossComponent.h"
#include "Combat/BossBulletManagerComponent.h"
#include "Renderer/System/Core/BaseModel.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "RailMechanics/SplineFollowerComponent.h"

DroneManagerComponent::DroneManagerComponent() {}

DroneManagerComponent::~DroneManagerComponent() {
    RecallAllDrones();
}

void DroneManagerComponent::Initialize() {
    batchRenderer_ = gameObject_->GetComponent<ModelBatchRendererComponent>();
}

void DroneManagerComponent::Start() {
    if (auto scene = gameObject_->GetScene()) {
        player_ = scene->FindGameObject("Player");
    }
}

void DroneManagerComponent::Update() {
    if (!batchRenderer_) {
        batchRenderer_ = gameObject_->GetComponent<ModelBatchRendererComponent>();
        if (!batchRenderer_) {
            return;
        }
    }

    batchRenderer_->ClearInstances();

    activeDroneCount_ = static_cast<int>(activeDrones_.size());
    if (activeDrones_.empty()) {
        return;
    }

    auto bossObj = boss_.lock();
    if (!bossObj) {
        return;
    }

    auto bossTransform = bossObj->GetComponent<TransformComponent>();
    if (!bossTransform) {
        return;
    }

    Irufemi::Vector3 bossPos = bossTransform->GetWorldPosition();
    Irufemi::Vector3 bossRight = bossTransform->GetWorldRight();
    Irufemi::Vector3 bossUp = bossTransform->GetWorldUp();

    // プレイヤーの安全な取得フォールバック & レール走行慣性の取得（ガリレイ慣性補正）
    Irufemi::Vector3 playerPos = bossPos; // デフォルト
    Irufemi::Vector3 railVelocity = {0.0f, 0.0f, 0.0f};
    bool hasPlayer = false;

    if (player_.expired()) {
        if (auto scene = gameObject_->GetScene()) {
            player_ = scene->FindGameObject("Player");
            if (player_.expired()) {
                player_ = scene->FindGameObject("PlayerCart");
            }
        }
    }

    bool isCombatEngaged = true;
    if (auto player = player_.lock()) {
        if (auto playerTransform = player->GetComponent<TransformComponent>()) {
            playerPos = playerTransform->GetWorldPosition();
            hasPlayer = true;
        }

        // レール追従の走行速度ベクトルを抽出
        auto cart = player->GetParent() ? player->GetParent() : player;
        if (auto follower = cart->GetComponent<SplineFollowerComponent>()) {
            if (auto cartTransform = cart->GetComponent<TransformComponent>()) {
                railVelocity = cartTransform->GetWorldForward() * follower->GetSpeed();
            }
            // プレイヤーが演出中（負の距離、またはレール速度倍率が0.0f）の場合は戦闘未開戦として射撃を保留
            if (follower->GetCurrentDistance() < 0.0f || follower->GetSpeedMultiplier() <= 0.0f) {
                isCombatEngaged = false;
            }
        }
    }

    float deltaTime = GetEngine() ? GetEngine()->GetGameDeltaTime() : (1.0f / 60.0f);

    // Data-Oriented Update Loop (CPUキャッシュ効率化)
    for (size_t i = 0; i < activeDrones_.size();) {
        auto& drone = activeDrones_[i];

        if (!drone.gameObject || !drone.gameObject->GetIsActive()) {
            // 個別撃破等で非アクティブになった場合はプールへ返却し、リストから除外
            if (dronePool_ && drone.handle.IsValid()) {
                dronePool_->Release(drone.handle);
            }
            if (i + 1 < activeDrones_.size()) {
                activeDrones_[i] = std::move(activeDrones_.back());
                animDataList_[i] = std::move(animDataList_.back());
            }
            activeDrones_.pop_back();
            animDataList_.pop_back();
            continue;
        }

        auto& droneObj = drone.gameObject;
        auto& anim = animDataList_[i];

        // 1. 旋回角度の更新
        anim.orbitAngle += orbitSpeed_ * deltaTime;

        // 2. 座標の計算（ボスの機体姿勢に同期する局所基底リングフォーメーション）
        float x = std::cos(anim.orbitAngle) * orbitRadius_;
        float y = std::sin(anim.orbitAngle) * orbitRadius_;
        Irufemi::Vector3 targetPos = bossPos + bossRight * x + bossUp * y;

        // 3. 向きの計算 (ゼロベクトル・ゼロ除算ガード)
        Irufemi::Vector3 rot = {0.0f, 0.0f, 0.0f};
        if (hasPlayer) {
            Irufemi::Vector3 diff = playerPos - targetPos;
            if (diff.LengthSquared() > 1e-4f) {
                rot = Irufemi::Math::LookRotation(Irufemi::Math::Normalize(diff));
            }
        } else {
            Irufemi::Vector3 diff = bossPos - targetPos;
            if (diff.LengthSquared() > 1e-4f) {
                rot = Irufemi::Math::LookRotation(Irufemi::Math::Normalize(diff));
            }
        }

        // 4. 当たり判定（GameObject）のTransform更新（ワールド空間に明示設定）
        auto t = droneObj->GetComponent<TransformComponent>();
        if (t) {
            t->SetWorldPosition(targetPos);
            t->SetWorldRotation(rot);
        }

        // 5. 弾幕の発射処理 (戦闘開戦中のみタイマー加算 ＆ ガリレイ慣性補正)
        if (isCombatEngaged) {
            anim.fireTimer += deltaTime;
        }
        if (anim.fireTimer >= fireInterval_) {
            anim.fireTimer = 0.0f;
            if (bulletManager_ && hasPlayer) {
                Irufemi::Vector3 diff = playerPos - targetPos;
                if (diff.LengthSquared() > 1e-4f) {
                    Irufemi::Vector3 dirToPlayer = Irufemi::Math::Normalize(diff);
                    // レール相対速度ベクトル + レール走行慣性速度ベクトル
                    Irufemi::Vector3 bulletVelocity = dirToPlayer * bulletSpeed_ + railVelocity;
                    bulletManager_->SpawnBullet(targetPos, bulletVelocity);
                }
            }
        }

        // 6. GPUバッチ描画用インスタンスデータの登録
        Irufemi::Transform batchT;
        batchT.translate = targetPos;
        batchT.rotate = rot;
        batchT.scale = {1.0f, 1.0f, 1.0f}; // Prefabのスケーリングを適用する場合は変更
        if (t) {
            batchT.scale = t->GetScale();
        }
        int32_t effectType = 0;
        float effectParam = 0.0f;
        bool enableMask = false;
        if (auto effectMask = gameObject_->GetComponent<EffectMaskComponent>()) {
            enableMask = effectMask->GetEnableEffectMask();
            effectType = effectMask->GetCustomEffectType();
            effectParam = effectMask->GetCachedEffectParam();
            if (enableMask && effectParam == 0.0f) {
                effectMask->Update();
                effectParam = effectMask->GetCachedEffectParam();
            }
        }
        batchRenderer_->AddInstance(batchT, effectType, effectParam, enableMask);

        ++i;
    }
    activeDroneCount_ = static_cast<int>(activeDrones_.size());
}

void DroneManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Drone Prefab Path", &dronePrefabPath_).SetPrefabFilter("SphereColliderComponent");
    RegisterProperty("Max Drones", &maxDrones_);
    RegisterPropertyRange("Orbit Radius", &orbitRadius_, 0.0f, 100.0f);
    RegisterPropertyRange("Orbit Speed", &orbitSpeed_, 0.0f, 10.0f);
    RegisterPropertyRange("Fire Interval", &fireInterval_, 0.1f, 10.0f);
    RegisterPropertyRange("Bullet Speed", &bulletSpeed_, 10.0f, 150.0f);
    RegisterProperty("Active Drones (Batch)", &activeDroneCount_);
}

void DroneManagerComponent::DeployDrones(std::weak_ptr<GameObject> boss, int count,
                                         BossBulletManagerComponent* bulletMgr) {
    boss_ = boss;
    bulletManager_ = bulletMgr;

    if (!dronePool_) {
        auto factory = [this, boss]() {
            auto obj = GetGameObject()->Instantiate(dronePrefabPath_);
            if (obj) {
                obj->SetIsActive(false);
                obj->SetIsSerializable(false); // セーブデータ（InGame.json）への混入を防止

                // プレハブから動的生成されたドローンの弾がプレイヤーを追跡できるようIDを伝播する
                if (auto playerLock = player_.lock()) {
                    if (auto bulletMgr = obj->GetComponent<BossBulletManagerComponent>()) {
                        bulletMgr->SetTargetPlayerID(playerLock->GetInstanceID());
                    }
                }

                // プレハブを BossDroneManager (gameObject_) の直接の子として追加する
                gameObject_->AddChild(obj);
            }
            return obj;
        };
        dronePool_ = std::make_unique<ObjectPool<GameObject>>(maxDrones_, factory);
    }

    if (!dronePool_) {
        return;
    }

    float angleStep = (Irufemi::Math::PI * 2.0f) / count;

    for (int i = 0; i < count; ++i) {
        auto handle = dronePool_->Acquire();
        if (handle.IsValid()) {
            std::shared_ptr<GameObject> droneObj = dronePool_->Resolve(handle);
            if (droneObj) {
                droneObj->SetIsActive(true);
                activeDrones_.push_back({handle, droneObj});

                DroneAnimData anim;
                anim.orbitAngle = angleStep * i;
                anim.fireTimer = Irufemi::Random::GeneratorFloat(0.0f, fireInterval_);
                animDataList_.push_back(anim);
            } else {
                dronePool_->Release(handle);
            }
        }
    }
}

void DroneManagerComponent::RecallAllDrones() {
    if (!dronePool_) {
        return;
    }
    for (auto& drone : activeDrones_) {
        if (drone.gameObject) {
            drone.gameObject->SetIsActive(false);
        }
        dronePool_->Release(drone.handle);
    }
    activeDrones_.clear();
    animDataList_.clear();
}
