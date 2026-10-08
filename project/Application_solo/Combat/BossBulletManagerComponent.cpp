#include "Combat/BossBulletManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Framework/Scene/BaseScene.h"
#include "Player/GravityPlayerComponent.h"
#include "Player/PlayerHealthComponent.h"
#include "Combat/IDamageable.h"
#include "Environment/DestructibleEnvironmentComponent.h"
#include "Environment/DebrisComponent.h"
#include "Physics/CollisionManager.h"
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Core/Utility/Log.h"
#include "Effects/EffectManagerComponent.h"
#include <iostream>
#include <algorithm>
#include <limits>
BossBulletManagerComponent::BossBulletManagerComponent() {}

void BossBulletManagerComponent::Initialize() {
    virtualManager_ = gameObject_->GetComponent<VirtualEntityManagerComponent>();
    if (!virtualManager_) {
        virtualManager_ = gameObject_->AddComponent<VirtualEntityManagerComponent>().get();
    }

    // バッチレンダラに球モデルを設定
    if (auto batchRenderer = gameObject_->GetComponent<ModelBatchRendererComponent>()) {
        batchRenderer->LoadModel("resources/model/BossDroneBullet/BossDroneBullet.obj");
        // TODO: マテリアル側での弾色設定（紫色等）に対応後、マテリアルパラメータを反映する
    }

    auto factory = []() -> std::shared_ptr<GameObject> { return nullptr; };
    virtualManager_->Setup(0, maxBullets_, factory);

    bulletDataList_.resize(maxBullets_);
    while (!activeVirtualIds_.empty()) {
        activeVirtualIds_.pop();
    }
}

void BossBulletManagerComponent::Start() {
    // シーン開始時に依存マネージャーを事前解決（Updateループでの検索負荷を完全排除）
    if (auto scene = gameObject_->GetScene()) {
        if (auto go = scene->FindGameObject("EffectManager")) {
            effectManager_ = go->GetComponent<EffectManagerComponent>();
        }
    }
}

void BossBulletManagerComponent::Update() {
    if (!virtualManager_) {
        return;
    }

    float dt = GetEngine() ? GetEngine()->GetGameDeltaTime() : 0.0f;
    if (dt <= 0.0f) {
        return; // ポーズ中（TimeScale == 0）は弾幕更新を完全停止
    }

    int activeCount = static_cast<int>(activeVirtualIds_.size());
    if (activeCount == 0) {
        return;
    }

    // 共通の爆発エフェクト処理ラムダ（プレハブデータから自動再生）
    auto playExplosion = [&](const Irufemi::Vector3& pos) {
        if (effectManager_) {
            effectManager_->PlayEffect(hitEffectKey_, pos);
        }
    };

    // --- Phase 1: 弾の移動 & 寿命判定 & 弾幕クラスタAABBの算出 ---
    std::vector<int> survivedBulletVids;
    survivedBulletVids.reserve(activeCount);

    Irufemi::Vector3 clusterMin = {(std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(),
                                   (std::numeric_limits<float>::max)()};
    Irufemi::Vector3 clusterMax = {-(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)(),
                                   -(std::numeric_limits<float>::max)()};

    for (int i = 0; i < activeCount; ++i) {
        int vid = activeVirtualIds_.front();
        activeVirtualIds_.pop();

        auto& data = bulletDataList_[vid];
        data.lifeTimer -= dt;

        auto* vi = virtualManager_->GetInstance(vid);
        if (vi) {
            if (data.lifeTimer <= 0.0f) {
                playExplosion(vi->position);
                ReleaseBullet(vid);
                continue;
            }

            vi->position += data.velocity * dt;
            survivedBulletVids.push_back(vid);

            // クラスタAABBの拡張（hitRadius_分も含める）
            clusterMin.x = (std::min)(clusterMin.x, vi->position.x - hitRadius_);
            clusterMin.y = (std::min)(clusterMin.y, vi->position.y - hitRadius_);
            clusterMin.z = (std::min)(clusterMin.z, vi->position.z - hitRadius_);

            clusterMax.x = (std::max)(clusterMax.x, vi->position.x + hitRadius_);
            clusterMax.y = (std::max)(clusterMax.y, vi->position.y + hitRadius_);
            clusterMax.z = (std::max)(clusterMax.z, vi->position.z + hitRadius_);
        }
    }

    if (survivedBulletVids.empty()) {
        return;
    }

    // --- Phase 2: 弾幕クラスタAABBによるBVH事前フェッチ（1フレームに1回のみ） ---
    auto engine = GetEngine();
    auto cm = engine ? engine->GetCollisionManager() : nullptr;

    struct CachedTargetProxy {
        ColliderComponent* collider = nullptr;
        GameObject* gameObject = nullptr;
        PlayerHealthComponent* playerHealth = nullptr;
        DestructibleEnvironmentComponent* destructible = nullptr;
        DebrisComponent* debris = nullptr;
        bool isEnvironment = false;
        bool isSphere = false;
        Irufemi::Sphere sphere{};
        Irufemi::AABB broadAABB{};
    };

    std::vector<CachedTargetProxy> targetProxies;

    if (cm) {
        Irufemi::AABB clusterAABB{clusterMin, clusterMax};
        prefetchedColliders_.clear();
        cm->QueryAABB(clusterAABB, prefetchedColliders_);

        targetProxies.reserve(prefetchedColliders_.size());
        for (auto col : prefetchedColliders_) {
            if (!col) {
                continue;
            }
            auto obj = col->GetGameObject();
            if (!obj || !obj->GetIsActive() || obj->IsDestroyed()) {
                continue;
            }

            // 被弾対象となるコンポーネントをチェック
            auto healthComp = obj->GetComponent<PlayerHealthComponent>();
            auto destructibleComp = obj->GetComponent<DestructibleEnvironmentComponent>();
            auto debrisComp = obj->GetComponent<DebrisComponent>();
            bool isEnv = (cm && (col->GetLayer() & cm->GetLayerMask("Environment")) != 0);

            // 自機シールド（Orbiting）以外のガレキは判定除外
            if (debrisComp && debrisComp->GetState() != DebrisState::Orbiting) {
                debrisComp = nullptr;
            }

            // プレイヤー、破壊可能物、自機シールド、環境壁のいずれでもない場合はスキップ
            if (!healthComp && !destructibleComp && !debrisComp && !isEnv) {
                continue;
            }

            CachedTargetProxy proxy{};
            proxy.collider = col;
            proxy.gameObject = obj;
            proxy.playerHealth = healthComp;
            proxy.destructible = destructibleComp;
            proxy.debris = debrisComp;
            proxy.isEnvironment = isEnv;

            if (auto sphereCol = dynamic_cast<SphereColliderComponent*>(col)) {
                proxy.isSphere = true;
                proxy.sphere = sphereCol->GetWorldSphere();
                float totalR = proxy.sphere.radius + hitRadius_;
                proxy.broadAABB.min = {proxy.sphere.center.x - totalR, proxy.sphere.center.y - totalR,
                                       proxy.sphere.center.z - totalR};
                proxy.broadAABB.max = {proxy.sphere.center.x + totalR, proxy.sphere.center.y + totalR,
                                       proxy.sphere.center.z + totalR};
            } else {
                proxy.isSphere = false;
                Irufemi::AABB bbox = col->GetBoundingBox();
                proxy.broadAABB.min = {bbox.min.x - hitRadius_, bbox.min.y - hitRadius_, bbox.min.z - hitRadius_};
                proxy.broadAABB.max = {bbox.max.x + hitRadius_, bbox.max.y + hitRadius_, bbox.max.z + hitRadius_};
            }

            targetProxies.push_back(proxy);
        }
    }

    // --- Phase 3: 弾 vs 事前フェッチされた被弾候補のバッチ判定 ---
    for (int vid : survivedBulletVids) {
        auto* vi = virtualManager_->GetInstance(vid);
        if (!vi) {
            continue;
        }
        const auto& p = vi->position;

#if defined(_DEBUG) || defined(DEVELOPMENT) || defined(EditorMode)
        if (cm && cm->GetIsDrawDebugLinePtr() && *cm->GetIsDrawDebugLinePtr()) {
            Irufemi::Vector3 minPos = {p.x - hitRadius_, p.y - hitRadius_, p.z - hitRadius_};
            Irufemi::Vector3 maxPos = {p.x + hitRadius_, p.y + hitRadius_, p.z + hitRadius_};
            cm->DrawDebugAABB(Irufemi::AABB{minPos, maxPos}, {1.0f, 0.0f, 0.0f, 1.0f}, DebugCategory::Combat);
        }
#endif

        bool isHit = false;
        if (!targetProxies.empty()) {
            for (const auto& proxy : targetProxies) {
                // 1. Broadphase: ターゲットの拡張AABB内かチェック
                if (p.x < proxy.broadAABB.min.x || p.x > proxy.broadAABB.max.x || p.y < proxy.broadAABB.min.y ||
                    p.y > proxy.broadAABB.max.y || p.z < proxy.broadAABB.min.z || p.z > proxy.broadAABB.max.z) {
                    continue;
                }

                // 2. Narrowphase
                if (proxy.isSphere) {
                    float totalR = proxy.sphere.radius + hitRadius_;
                    float dx = p.x - proxy.sphere.center.x;
                    float dy = p.y - proxy.sphere.center.y;
                    float dz = p.z - proxy.sphere.center.z;
                    if (dx * dx + dy * dy + dz * dz > totalR * totalR) {
                        continue;
                    }
                }

                // 命中確定！
                if (proxy.playerHealth) {
                    const bool isTarget =
                        (targetPlayerID_ == 0 || proxy.gameObject->GetInstanceID() == targetPlayerID_);
                    if (isTarget) {
                        if (!proxy.playerHealth->IsInvincible()) {
                            proxy.playerHealth->TakeDamage(1);
                        }
                        // 無敵時間中も弾を消費・消滅させて素通りを防止
                        isHit = true;
                        break;
                    }
                } else if (proxy.destructible) {
                    // 環境物破壊
                    proxy.destructible->TakeDamage(1);
                    isHit = true;
                    break;
                } else if (proxy.debris || proxy.isEnvironment) {
                    // 自機シールドまたは壁に着弾して消滅
                    isHit = true;
                    break;
                }
            }
        }

        if (isHit) {
            playExplosion(vi->position);
            ReleaseBullet(vid);
        } else {
            activeVirtualIds_.push(vid); // 生存弾をキューに維持
        }
    }
}

void BossBulletManagerComponent::OnRegisterProperties() {
    RegisterProperty("Max Bullets", &maxBullets_);
    RegisterProperty("Default Life Time", &defaultLifeTime_);
    RegisterPropertyRange("Hit Radius", &hitRadius_, 0.1f, 10.0f);
    RegisterProperty("Hit Effect Key", &hitEffectKey_);
    RegisterGameObjectRef("Target Player", &targetPlayerID_);
}

void BossBulletManagerComponent::OnIDRemapped(const std::unordered_map<uint64_t, uint64_t>& idMap) {
    if (targetPlayerID_ != 0) {
        auto it = idMap.find(targetPlayerID_);
        if (it != idMap.end()) {
            targetPlayerID_ = it->second;
        }
    }
}

void BossBulletManagerComponent::SpawnBullet(const Irufemi::Vector3& position, const Irufemi::Vector3& velocity) {
    if (!virtualManager_) {
        return;
    }

    int vid = virtualManager_->AddVirtualInstance(position, {0, 0, 0}, bulletScale_);
    if (vid >= 0 && vid < maxBullets_) {
        BossBulletData data;
        data.velocity = velocity;
        data.lifeTimer = defaultLifeTime_;
        bulletDataList_[vid] = data;
        activeVirtualIds_.push(vid);
    }
}

void BossBulletManagerComponent::ReleaseBullet(int virtualId) {
    if (virtualManager_) {
        virtualManager_->RemoveVirtualInstance(virtualId);
    }
}
