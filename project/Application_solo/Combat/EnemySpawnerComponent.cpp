#include "Combat/EnemySpawnerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Scene/BaseScene.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/Renderer/MeshRendererComponent.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "RailMechanics/RailShooterEnemyComponent.h"
#include "Core/Math/Random/Random.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Player/TargetableComponent.h"
#include "Framework/Prefab/PrefabUtility.h"
#include "Core/Math/MathFunction.h"
#include "Environment/DebrisManagerComponent.h"
#include "Effects/EffectManagerComponent.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"
#include "Framework/Component/Effect/EffectMaskComponent.h"
#include "Combat/ScoreManagerComponent.h"
#include "Renderer/PostProcess/PostProcessManager.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"

// データ指向設計とインスタンシング（Data-Oriented Design & Instancing）:
// 個々の敵オブジェクトにMeshRendererを持たせるのではなく、Spawnerが一括でModelBatchRendererComponentを管理します。
// これにより、大量の敵を描画する際でもドローコールが削減され、描画負荷を低減します。

EnemySpawnerComponent::~EnemySpawnerComponent() {
    for (auto& pair : prefabPools_) {
        if (pair.second && pair.second->pool) {
            pair.second->pool->ForEach([](const std::shared_ptr<Irufemi::GameObject>& enemy) {
                if (enemy) {
                    if (auto enemyComp = enemy->GetComponent<RailShooterEnemyComponent>()) {
                        enemyComp->SetOnDeathCallback(nullptr);
                        enemyComp->SetOnDespawnListener(nullptr);
                    }
                }
            });
        }
    }
}

void EnemySpawnerComponent::Initialize() {}

std::shared_ptr<Irufemi::GameObject> EnemySpawnerComponent::GetPlayerCartObject() {
    auto obj = playerCartObj_.lock();
    if (!obj) {
        if (auto scene = gameObject_ ? gameObject_->GetScene() : nullptr) {
            obj = scene->FindGameObject("PlayerCart");
            if (!obj) {
                obj = scene->FindGameObject("Player");
            }
            if (obj) {
                playerCartObj_ = obj;
            }
        }
    }
    return obj;
}

std::shared_ptr<Irufemi::GameObject> EnemySpawnerComponent::GetDebrisManagerObject() {
    auto obj = debrisManagerObj_.lock();
    if (!obj) {
        if (auto scene = gameObject_ ? gameObject_->GetScene() : nullptr) {
            obj = scene->FindGameObject("DebrisManager");
            if (obj) {
                debrisManagerObj_ = obj;
            }
        }
    }
    return obj;
}

void EnemySpawnerComponent::OnRegisterProperties() {
    // プレハブ内にモデル・スケール・コライダー・パラメータ等の全情報が一元化されているため、
    // インスペクターにはプレハブ選択・編集プロパティのみを公開
    RegisterProperty("Enemy Prefab Path", &enemyPrefabPath_)
        .SetTooltip("スポーン対象の敵プレハブアセットを選択")
        .SetPrefabFilter("RailShooterEnemyComponent"); // 敵コンポーネントを持つプレハブのみに型安全フィルタリング
}

void EnemySpawnerComponent::Start() {
    // 依存 GameObject の事前キャッシュ
    GetPlayerCartObject();
    GetDebrisManagerObject();

    // 1. ステージで使用される全プレハブの事前プール生成（GPU Instancing & オブジェクトプール事前確保）
    const std::vector<std::pair<std::string, uint32_t>> preloadConfigs = {
        {enemyPrefabPath_, 20},
        {"resources/prefabs/Enemy_DiveDrone.json", 30},
        {"resources/prefabs/Enemy_SniperArtillery.json", 15}};

    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    auto voxelMgr = (scene && scene->GetEngine()) ? scene->GetEngine()->GetVoxelParticleManager() : nullptr;

    for (const auto& [prefabPath, poolSize] : preloadConfigs) {
        if (prefabPath.empty()) {
            continue;
        }

        auto* poolData = GetOrCreatePrefabPool(prefabPath, poolSize);

        // 2. エンジン既存のボクセル化プール事前確保APIを呼び出し、ロード画面中に完了待機させる
        if (poolData && voxelMgr && !poolData->modelPath.empty()) {
            voxelMgr->ReservePool(poolData->modelPath, {3, 3, 3}, 8);
        }
    }
}

void EnemySpawnerComponent::PreloadPrefab(const std::string& prefabPath, uint32_t poolSize) {
    if (prefabPath.empty()) {
        return;
    }
    auto* poolData = GetOrCreatePrefabPool(prefabPath, poolSize);
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    auto voxelMgr = (scene && scene->GetEngine()) ? scene->GetEngine()->GetVoxelParticleManager() : nullptr;
    if (poolData && voxelMgr && !poolData->modelPath.empty()) {
        voxelMgr->ReservePool(poolData->modelPath, {3, 3, 3}, 8);
    }
}

EnemySpawnerComponent::PrefabPoolData* EnemySpawnerComponent::GetOrCreatePrefabPool(const std::string& prefabPath,
                                                                                    uint32_t poolSize) {
    if (prefabPath.empty()) {
        return nullptr;
    }

    auto it = prefabPools_.find(prefabPath);
    if (it != prefabPools_.end() && it->second) {
        return it->second.get();
    }

    auto poolData = std::make_unique<PrefabPoolData>();
    poolData->prefabPath = prefabPath;

    // プレハブ（Archetype）からモデル・基本スケール・当たり判定半径を自動解決
    auto metrics = Irufemi::PrefabUtility::ExtractMetrics(prefabPath);
    if (!metrics.modelPath.empty()) {
        poolData->modelPath = metrics.modelPath;
    } else {
        poolData->modelPath = enemyModelPath_;
    }
    poolData->baseScale = metrics.baseScale;
    poolData->hasOBBCollider = metrics.hasOBBCollider;
    if (metrics.hasOBBCollider) {
        poolData->baseOBBSize = metrics.obbSize;
    }
    if (metrics.hasSphereCollider) {
        poolData->baseColliderRadius = metrics.colliderRadius > 0.0f ? metrics.colliderRadius : 1.0f;
    } else {
        poolData->baseColliderRadius = 1.0f;
    }

    // プレハブ専用の子 GameObject を作成して ModelBatchRendererComponent をアタッチ（1オブジェクト1コンポーネント原則）
    if (gameObject_) {
        auto batchChild = std::make_shared<Irufemi::GameObject>("BatchRenderer_" + prefabPath);
        gameObject_->AddChild(batchChild);
        poolData->batchRenderer = batchChild->AddComponent<Irufemi::ModelBatchRendererComponent>();
        poolData->batchRenderer->LoadModel(poolData->modelPath);
    }

    auto weakObj = gameObject_ ? gameObject_->weak_from_this() : std::weak_ptr<Irufemi::GameObject>();
    std::string capturedPrefabPath = prefabPath;
    std::string capturedModelPath = poolData->modelPath;
    Irufemi::Vector3 capturedBaseScale = poolData->baseScale;
    float capturedColliderRadius = poolData->baseColliderRadius;
    Irufemi::Vector3 capturedOBBSize = poolData->baseOBBSize;

    uint32_t effectivePoolSize = (poolSize > 0) ? poolSize : static_cast<uint32_t>(maxEnemies_);

    poolData->pool = std::make_unique<Irufemi::ObjectPool<Irufemi::GameObject>>(
        effectivePoolSize,
        [weakObj, capturedPrefabPath, capturedModelPath, capturedBaseScale, capturedColliderRadius, capturedOBBSize]() {
            std::shared_ptr<Irufemi::GameObject> enemy = nullptr;
            if (auto spawnerObj = weakObj.lock()) {
                enemy = spawnerObj->Instantiate(capturedPrefabPath);
            }
            if (!enemy) {
                enemy = std::make_shared<Irufemi::GameObject>("Enemy");
                enemy->AddComponent<RailShooterEnemyComponent>();
            }

            enemy->SetIsSerializable(false); // セーブデータへの混入を防止

            if (auto spawnerObj = weakObj.lock()) {
                spawnerObj->AddChild(enemy);
            }

            // プレハブ個別の MeshRenderer は削除し、Spawner側の ModelBatchRenderer（GPU Instancing）で一括描画
            if (auto meshRenderer = enemy->GetComponent<Irufemi::MeshRendererComponent>()) {
                enemy->RemoveComponent(meshRenderer);
            }

            if (auto transform = enemy->GetTransform()) {
                transform->SetScale(capturedBaseScale);
            }

            if (auto collider = enemy->GetComponent<Irufemi::SphereColliderComponent>()) {
                collider->SetLocalRadius(capturedColliderRadius);
            } else if (auto obb = enemy->GetComponent<Irufemi::OBBColliderComponent>()) {
                obb->SetLocalSize(capturedOBBSize);
            }

            if (auto enemyComp = enemy->GetComponent<RailShooterEnemyComponent>()) {
                enemyComp->Initialize(); // ロード画面中にGPUリソースや事前コンポーネントを先行確立
                enemyComp->SetOnDespawnListener(
                    [weakObj, capturedModelPath](Irufemi::GameObject* deadObj, DespawnReason reason) {
                        if (!deadObj) {
                            return;
                        }

                        // プレイヤー撃破（または自爆体当たり）時のみ、残骸ガレキドロップ＆Voxel破砕演出を実行
                        if (reason == DespawnReason::KilledByPlayer || reason == DespawnReason::CollisionSuicide) {
                            auto deadTrans = deadObj->GetTransform();
                            Irufemi::Vector3 deadPos =
                                deadTrans ? deadTrans->GetWorldPosition() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
                            Irufemi::Vector3 currentScale =
                                deadTrans ? deadTrans->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

                            auto scene = deadObj->GetScene();
                            if (scene) {
                                // 1. プレイヤー位置の取得（キャッシュ優先）
                                Irufemi::Vector3 playerPos = deadPos;
                                std::shared_ptr<Irufemi::GameObject> playerObj = nullptr;
                                std::shared_ptr<Irufemi::GameObject> debrisMgrObj = nullptr;
                                if (auto spawnerObj = weakObj.lock()) {
                                    if (auto spawner = spawnerObj->GetComponent<EnemySpawnerComponent>()) {
                                        playerObj = spawner->GetPlayerCartObject();
                                        debrisMgrObj = spawner->GetDebrisManagerObject();
                                    }
                                }
                                if (!playerObj) {
                                    playerObj = scene->FindGameObject("PlayerCart");
                                    if (!playerObj) {
                                        playerObj = scene->FindGameObject("Player");
                                    }
                                }
                                if (playerObj && playerObj->GetTransform()) {
                                    playerPos = playerObj->GetTransform()->GetWorldPosition();
                                }

                                // 2. スケール連動のドロップ個数決定（中型機は大量ドロップボーナス）
                                float scaleMult = 1.0f;
                                if (auto ec = deadObj->GetComponent<RailShooterEnemyComponent>()) {
                                    scaleMult = ec->GetScaleMultiplier();
                                }
                                constexpr float kLargeEnemyScaleThreshold = 1.3f;
                                constexpr int kLargeEnemyDropCount = 4;
                                constexpr int kNormalEnemyDropCount = 3;
                                int dropCount = (scaleMult >= kLargeEnemyScaleThreshold) ? kLargeEnemyDropCount
                                                                                         : kNormalEnemyDropCount;

                                // 3. 自機手前方向への物理バーストドロップ（キャッシュ優先）
                                if (!debrisMgrObj) {
                                    debrisMgrObj = scene->FindGameObject("DebrisManager");
                                }
                                if (debrisMgrObj) {
                                    if (auto debrisMgr = debrisMgrObj->GetComponent<DebrisManagerComponent>()) {
                                        constexpr float kDebrisBurstSpreadMin = 3.5f;
                                        constexpr float kDebrisBurstSpeed = 12.0f;
                                        debrisMgr->SpawnDebrisBurst(deadPos, playerPos, dropCount,
                                                                    kDebrisBurstSpreadMin, kDebrisBurstSpeed);
                                    }
                                }

                                // 4. 敵機体のVoxelメッシュ破砕演出（プレハブ固有のモデルメッシュから破砕）
                                if (auto engine = scene->GetEngine()) {
                                    if (auto voxelMgr = engine->GetVoxelParticleManager()) {
                                        constexpr float kVoxelParticleLifeTime = 1.6f;
                                        constexpr float kVoxelParticleGravity = 3.5f;
                                        constexpr float kVoxelParticleDispersion = 24.0f;

                                        Irufemi::VoxelEmitter p{};
                                        p.particleType = 5; // DebrisExplosive
                                        p.lifeTime = kVoxelParticleLifeTime;
                                        p.gravity = kVoxelParticleGravity;
                                        p.dispersion = kVoxelParticleDispersion;
                                        p.scale = {0.65f, 0.65f, 0.65f};
                                        p.startColor = {14.0f, 3.0f, 12.0f, 1.0f}; // 超高輝度マゼンタ過電圧放電色
                                        p.endColor = {0.2f, 0.02f, 0.25f, 0.0f}; // 電磁紫煙フェード
                                        p.dissolveEdgeColor = {30.0f, 6.0f, 26.0f, 1.0f}; // 超高圧放電エッジ発光
                                        voxelMgr->PlayExplosion(capturedModelPath, deadPos, {0.0f, 0.0f, 0.0f},
                                                                {0.0f, 0.0f, 0.0f}, currentScale, p, {3, 3, 3});
                                    }
                                }

                                // 5. 撃破爆発エフェクトの再生（高圧電磁破裂）
                                if (auto effectMgr = EffectManagerComponent::GetInstance()) {
                                    effectMgr->PlayEffect("EnemyExplosion", deadPos);
                                }

                                // 6. スコア＆マルチキル通知（基本点100pts）
                                if (auto scoreMgr = ScoreManagerComponent::GetInstance()) {
                                    scoreMgr->NotifyEnemyKilled(100);
                                }
                            }
                        }

                        // プールへの返却
                        deadObj->SetIsActive(false);
                        if (auto spawnerObj = weakObj.lock()) {
                            if (auto spawner = spawnerObj->GetComponent<EnemySpawnerComponent>()) {
                                auto it = spawner->activeEnemyHandles_.find(deadObj->GetInstanceID());
                                if (it != spawner->activeEnemyHandles_.end()) {
                                    const std::string& pPath = it->second.prefabPath;
                                    auto pPoolIt = spawner->prefabPools_.find(pPath);
                                    if (pPoolIt != spawner->prefabPools_.end() && pPoolIt->second &&
                                        pPoolIt->second->pool) {
                                        pPoolIt->second->pool->Release(it->second.handle);
                                    }
                                    spawner->activeEnemyHandles_.erase(it);
                                }
                            }
                        }
                    });
            }

            enemy->SetIsActive(false);
            return enemy;
        });

    PrefabPoolData* result = poolData.get();
    prefabPools_[prefabPath] = std::move(poolData);
    return result;
}

void EnemySpawnerComponent::Update() {
    auto engine = GetEngine();
    if (engine && engine->IsSimulationPaused()) {
        return; // ポーズ中は前フレームのバッチインスタンスをそのまま保持
    }

    // 1. 各プレハブ専用の ModelBatchRendererComponent インスタンス描画キューをクリア
    for (auto& pair : prefabPools_) {
        if (pair.second && pair.second->batchRenderer) {
            pair.second->batchRenderer->ClearInstances();
        }
    }

    // 2. アクティブなすべての敵のワールド行列を、該当プレハブのバッチレンダラーへ一括登録
    for (const auto& pair : activeEnemyHandles_) {
        const auto& info = pair.second;
        if (info.enemyObj && info.enemyObj->GetIsActive()) {
            const std::string& prefabPath = info.prefabPath;
            auto poolIt = prefabPools_.find(prefabPath);
            if (poolIt != prefabPools_.end() && poolIt->second && poolIt->second->batchRenderer) {
                if (info.transform) {
                    int32_t effectType = 0;
                    float effectParam = 0.0f;
                    bool enableMask = false;
                    if (info.effectMask) {
                        enableMask = info.effectMask->GetEnableEffectMask();
                        effectType = info.effectMask->GetCustomEffectType();
                        effectParam = info.effectMask->GetCachedEffectParam();
                        if (enableMask && effectParam == 0.0f) {
                            info.effectMask->Update();
                            effectParam = info.effectMask->GetCachedEffectParam();
                        }
                    }
                    poolIt->second->batchRenderer->AddInstanceWorld(info.transform->GetWorldMatrix(), effectType,
                                                                    effectParam, enableMask);
                }
            }
        }
    }

#if defined(_DEBUG)
    auto input = engine ? engine->GetInputManager() : nullptr;
    if (!input) {
        return;
    }

    // '2'キーで敵をスポーン（デバッグ用）
    if (input->IsKeyPressed('2')) {
        constexpr float kDefaultSpawnForwardDistance = 50.0f;
        constexpr float kSpawnRangeX = 10.0f;
        constexpr float kSpawnRangeY = 5.0f;

        Irufemi::Vector3 spawnPos = {0.0f, 0.0f, kDefaultSpawnForwardDistance};
        Irufemi::Vector3 spawnRot = {0.0f, Irufemi::Math::PI, 0.0f};

        auto scene = gameObject_->GetScene();
        if (scene) {
            auto playerObj = scene->FindGameObject("Player");
            if (playerObj) {
                if (auto transform = playerObj->GetComponent<Irufemi::TransformComponent>()) {
                    spawnPos = transform->GetWorldPosition();
                    auto forward = transform->GetWorldForward();
                    spawnPos.x += forward.x * kDefaultSpawnForwardDistance;
                    spawnPos.y += forward.y * kDefaultSpawnForwardDistance;
                    spawnPos.z += forward.z * kDefaultSpawnForwardDistance;

                    auto right = transform->GetWorldRight();
                    auto up = transform->GetWorldUp();

                    float randX = Irufemi::Random::GeneratorFloat(-kSpawnRangeX, kSpawnRangeX);
                    float randY = Irufemi::Random::GeneratorFloat(-kSpawnRangeY, kSpawnRangeY);

                    spawnPos.x += right.x * randX + up.x * randY;
                    spawnPos.y += right.y * randX + up.y * randY;
                    spawnPos.z += right.z * randX + up.z * randY;

                    spawnRot = transform->GetWorldRotation();
                    spawnRot.y += Irufemi::Math::PI;
                }
            }
        }

        SpawnEnemy(spawnPos, spawnRot);
    }
#endif
}

Irufemi::GameObject* EnemySpawnerComponent::SpawnEnemy(const Irufemi::Vector3& position,
                                                       const Irufemi::Vector3& rotation, float scaleMultiplier) {
    return SpawnEnemyByPrefab(enemyPrefabPath_, position, rotation, scaleMultiplier);
}

Irufemi::GameObject* EnemySpawnerComponent::SpawnEnemyByPrefab(const std::string& prefabPath,
                                                               const Irufemi::Vector3& position,
                                                               const Irufemi::Vector3& rotation,
                                                               float scaleMultiplier) {
    auto poolData = GetOrCreatePrefabPool(prefabPath);
    if (!poolData || !poolData->pool) {
        return nullptr;
    }

    auto handle = poolData->pool->Acquire();
    if (!handle.IsValid()) {
        return nullptr;
    }

    auto enemy = poolData->pool->Resolve(handle);
    if (enemy) {
        auto transform = enemy->GetComponent<Irufemi::TransformComponent>();
        auto effectMask = enemy->GetComponent<Irufemi::EffectMaskComponent>();
        activeEnemyHandles_[enemy->GetInstanceID()] = {enemy.get(), transform, effectMask, prefabPath, handle};

        if (transform) {
            transform->SetWorldPosition(position);
            transform->SetWorldRotation(rotation);
            transform->SetScale(poolData->baseScale * scaleMultiplier);
        }

        // コライダーのローカルサイズ設定（プレハブアセットの設定値を忠実に維持）
        // スケーリングは上記 TransformComponent::SetScale により、GetWorldSphere() / GetWorldOBB() で
        // 自動的に worldScale が乗算されるため、ローカル値への scaleMultiplier 二重掛けは行わない
        if (auto collider = enemy->GetComponent<Irufemi::SphereColliderComponent>()) {
            collider->SetLocalRadius(poolData->baseColliderRadius);
        } else if (auto obb = enemy->GetComponent<Irufemi::OBBColliderComponent>()) {
            obb->SetLocalSize(poolData->baseOBBSize);
        }

        if (auto enemyComp = enemy->GetComponent<RailShooterEnemyComponent>()) {
            enemyComp->Initialize();
            enemyComp->SetScaleMultiplier(scaleMultiplier);
        }

        enemy->SetIsActive(true);
        return enemy.get();
    }

    return nullptr;
}
