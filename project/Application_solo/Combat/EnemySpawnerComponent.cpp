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
#include "Player/TargetableComponent.h"
#include "Framework/Prefab/PrefabUtility.h"
#include "Core/Math/MathFunction.h"
#include "Environment/DebrisManagerComponent.h"
#include "Effects/EffectManagerComponent.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"

// AAAタイトルのアプローチ (Data-Oriented Design & Instancing)
// 個々の敵オブジェクトにMeshRendererを持たせるのではなく、Spawnerが一括でModelBatchRendererComponentを管理します。
// これにより、数千体の敵を描画する際でもドローコールが1回（Instancing）に削減され、
// CPUとGPUのオーバーヘッドが劇的に改善されます（Unreal EngineのHISMやUnityのDOTSに近いアーキテクチャ）。

EnemySpawnerComponent::~EnemySpawnerComponent() {
    for (auto& pair : prefabPools_) {
        if (pair.second && pair.second->pool) {
            pair.second->pool->ForEach([](const std::shared_ptr<GameObject>& enemy) {
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

void EnemySpawnerComponent::OnRegisterProperties() {
    RegisterProperty("Enemy Model Path", &enemyModelPath_);
    RegisterProperty("Enemy Prefab Path", &enemyPrefabPath_);
    RegisterProperty("Base Enemy Scale", &baseEnemyScale_);
    RegisterProperty("Base Collider Radius", &baseColliderRadius_);
}

void EnemySpawnerComponent::Start() {
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
    auto metrics = PrefabUtility::ExtractMetrics(prefabPath);
    if (!metrics.modelPath.empty()) {
        poolData->modelPath = metrics.modelPath;
    } else {
        poolData->modelPath = enemyModelPath_;
    }
    poolData->baseScale = metrics.baseScale;
    if (metrics.hasSphereCollider) {
        poolData->baseColliderRadius = (std::max)(metrics.colliderRadius, 1.3f);
    } else {
        poolData->baseColliderRadius = 1.6f;
    }

    // プレハブ専用の ModelBatchRendererComponent をアタッチしてモデルをロード（Instancing描画）
    if (gameObject_) {
        poolData->batchRenderer = gameObject_->AddComponent<ModelBatchRendererComponent>();
        poolData->batchRenderer->LoadModel(poolData->modelPath);
    }

    auto weakObj = gameObject_ ? gameObject_->weak_from_this() : std::weak_ptr<GameObject>();
    std::string capturedPrefabPath = prefabPath;
    std::string capturedModelPath = poolData->modelPath;
    Irufemi::Vector3 capturedBaseScale = poolData->baseScale;
    float capturedColliderRadius = poolData->baseColliderRadius;

    uint32_t effectivePoolSize = (poolSize > 0) ? poolSize : static_cast<uint32_t>(maxEnemies_);

    poolData->pool = std::make_unique<ObjectPool<GameObject>>(effectivePoolSize, [weakObj, capturedPrefabPath,
                                                                                  capturedModelPath, capturedBaseScale,
                                                                                  capturedColliderRadius]() {
        std::shared_ptr<GameObject> enemy = nullptr;
        if (auto spawnerObj = weakObj.lock()) {
            enemy = spawnerObj->Instantiate(capturedPrefabPath);
        }
        if (!enemy) {
            enemy = std::make_shared<GameObject>("Enemy");
            enemy->AddComponent<RailShooterEnemyComponent>();
        }

        enemy->SetIsSerializable(false); // セーブデータへの混入を防止

        if (auto spawnerObj = weakObj.lock()) {
            spawnerObj->AddChild(enemy);
        }

        // プレハブ個別の MeshRenderer は削除し、Spawner側の ModelBatchRenderer（GPU Instancing）で一括描画
        if (auto meshRenderer = enemy->GetComponent<MeshRendererComponent>()) {
            enemy->RemoveComponent(meshRenderer);
        }

        if (auto transform = enemy->GetTransform()) {
            transform->SetScale(capturedBaseScale);
        }

        if (auto collider = enemy->GetComponent<SphereColliderComponent>()) {
            collider->SetLocalRadius(capturedColliderRadius);
        }

        if (auto enemyComp = enemy->GetComponent<RailShooterEnemyComponent>()) {
            enemyComp->SetOnDespawnListener([weakObj, capturedModelPath](GameObject* deadObj, DespawnReason reason) {
                if (!deadObj) {
                    return;
                }

                // プレイヤー撃破（または自爆体当たり）時のみ、残骸ガレキドロップ＆Voxel破砕演出を実行
                if (reason == DespawnReason::KilledByPlayer || reason == DespawnReason::CollisionSuicide) {
                    auto deadTrans = deadObj->GetComponent<TransformComponent>();
                    Irufemi::Vector3 deadPos =
                        deadTrans ? deadTrans->GetWorldPosition() : Irufemi::Vector3{0.0f, 0.0f, 0.0f};
                    Irufemi::Vector3 currentScale =
                        deadTrans ? deadTrans->GetScale() : Irufemi::Vector3{1.0f, 1.0f, 1.0f};

                    auto scene = deadObj->GetScene();
                    if (scene) {
                        // 1. プレイヤー位置の取得
                        Irufemi::Vector3 playerPos = deadPos;
                        auto playerObj = scene->FindGameObject("PlayerCart");
                        if (!playerObj) {
                            playerObj = scene->FindGameObject("Player");
                        }
                        if (playerObj && playerObj->GetTransform()) {
                            playerPos = playerObj->GetTransform()->GetWorldPosition();
                        }

                        // 2. スケール連動のドロップ個数決定（中型機は大量ドロップボーナス）
                        float scaleMult = 1.0f;
                        if (auto ec = deadObj->GetComponent<RailShooterEnemyComponent>()) {
                            scaleMult = ec->GetScaleMultiplier();
                        }
                        int dropCount = (scaleMult >= 1.3f) ? 4 : 2;

                        // 3. 自機手前方向への物理バーストドロップ
                        if (auto debrisMgrObj = scene->FindGameObject("DebrisManager")) {
                            if (auto debrisMgr = debrisMgrObj->GetComponent<DebrisManagerComponent>()) {
                                debrisMgr->SpawnDebrisBurst(deadPos, playerPos, dropCount, 3.5f, 12.0f);
                            }
                        }

                        // 4. 敵機体のVoxelメッシュ破砕演出（プレハブ固有のモデルメッシュから破砕）
                        if (auto engine = scene->GetEngine()) {
                            if (auto voxelMgr = engine->GetVoxelParticleManager()) {
                                VoxelEmitter p{};
                                p.particleType = 5; // DebrisExplosive
                                p.lifeTime = 1.2f;
                                p.gravity = 4.0f;
                                p.dispersion = 14.0f;
                                p.scale = {0.5f, 0.5f, 0.5f};
                                p.startColor = {1.8f, 1.3f, 0.9f, 1.0f};
                                p.endColor = {0.1f, 0.1f, 0.1f, 1.0f};
                                voxelMgr->PlayExplosion(capturedModelPath, deadPos, {0.0f, 0.0f, 0.0f},
                                                        {0.0f, 0.0f, 0.0f}, currentScale, p, {3, 3, 3});
                            }
                        }

                        // 5. 撃破爆発エフェクトの再生
                        if (auto effectMgr = EffectManagerComponent::GetInstance()) {
                            effectMgr->PlayEffect("debris_dust_effect", deadPos);
                        }
                    }
                }

                // プールへの返却
                deadObj->SetIsActive(false);
                if (auto spawnerObj = weakObj.lock()) {
                    if (auto spawner = spawnerObj->GetComponent<EnemySpawnerComponent>()) {
                        auto it = spawner->activeEnemyHandles_.find(deadObj);
                        if (it != spawner->activeEnemyHandles_.end()) {
                            const std::string& pPath = it->second.first;
                            auto pPoolIt = spawner->prefabPools_.find(pPath);
                            if (pPoolIt != spawner->prefabPools_.end() && pPoolIt->second && pPoolIt->second->pool) {
                                pPoolIt->second->pool->Release(it->second.second);
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
    // 1. 各プレハブ専用の ModelBatchRendererComponent インスタンス描画キューをクリア
    for (auto& pair : prefabPools_) {
        if (pair.second && pair.second->batchRenderer) {
            pair.second->batchRenderer->ClearInstances();
        }
    }

    // 2. アクティブなすべての敵のワールド行列を、該当プレハブのバッチレンダラーへ一括登録
    for (const auto& pair : activeEnemyHandles_) {
        GameObject* enemyObj = pair.first;
        if (enemyObj && enemyObj->GetIsActive()) {
            const std::string& prefabPath = pair.second.first;
            auto poolIt = prefabPools_.find(prefabPath);
            if (poolIt != prefabPools_.end() && poolIt->second && poolIt->second->batchRenderer) {
                if (auto transform = enemyObj->GetTransform()) {
                    poolIt->second->batchRenderer->AddInstanceWorld(transform->GetWorldMatrix());
                }
            }
        }
    }

    auto engine = GetEngine();
    auto input = engine ? engine->GetInputManager() : nullptr;
    if (!input) {
        return;
    }

    // '2'キーで敵をスポーン（デバッグ用）
    if (input->IsKeyPressed('2')) {
        Irufemi::Vector3 spawnPos = {0.0f, 0.0f, 50.0f};
        Irufemi::Vector3 spawnRot = {0.0f, Irufemi::Math::PI, 0.0f};

        auto scene = gameObject_->GetScene();
        if (scene) {
            auto playerObj = scene->FindGameObject("Player");
            if (playerObj) {
                if (auto transform = playerObj->GetComponent<TransformComponent>()) {
                    spawnPos = transform->GetWorldPosition();
                    auto forward = transform->GetWorldForward();
                    spawnPos.x += forward.x * 50.0f;
                    spawnPos.y += forward.y * 50.0f;
                    spawnPos.z += forward.z * 50.0f;

                    auto right = transform->GetWorldRight();
                    auto up = transform->GetWorldUp();

                    float randX = Irufemi::Random::GeneratorFloat(-10.0f, 10.0f);
                    float randY = Irufemi::Random::GeneratorFloat(-5.0f, 5.0f);

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
}

GameObject* EnemySpawnerComponent::SpawnEnemy(const Irufemi::Vector3& position, const Irufemi::Vector3& rotation,
                                              float scaleMultiplier) {
    return SpawnEnemyByPrefab(enemyPrefabPath_, position, rotation, scaleMultiplier);
}

GameObject* EnemySpawnerComponent::SpawnEnemyByPrefab(const std::string& prefabPath, const Irufemi::Vector3& position,
                                                      const Irufemi::Vector3& rotation, float scaleMultiplier) {
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
        activeEnemyHandles_[enemy.get()] = {prefabPath, handle};

        if (auto transform = enemy->GetComponent<TransformComponent>()) {
            transform->SetWorldPosition(position);
            transform->SetWorldRotation(rotation);
            transform->SetScale(poolData->baseScale * scaleMultiplier);
        }

        if (auto collider = enemy->GetComponent<SphereColliderComponent>()) {
            collider->SetLocalRadius(poolData->baseColliderRadius * scaleMultiplier);
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
