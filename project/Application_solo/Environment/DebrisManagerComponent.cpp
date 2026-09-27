#include "Environment/DebrisManagerComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Framework/Component/VirtualEntity/VirtualEntityManagerComponent.h"
#include "Framework/Component/Renderer/ModelBatchRendererComponent.h"
#include "Framework/Scene/SceneSerializer.h"
#include "Framework/Scene/BaseScene.h"
#include "Environment/DebrisComponent.h"
#include "Combat/IDamageable.h"
#include "Core/System/IrufemiEngine.h"
#include "Platform/Input/InputManager.h"
#include "Core/Math/Random/Random.h"
#include "Renderer/Camera/CameraManager.h"
#include "Renderer/Camera/Camera.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Renderer/PrimitiveRendererComponent.h"
#include "Renderer/Object/3D/Primitive/Primitive3DObject.h"
#include "Core/Type/PrimitiveType.h"
#include "Renderer/Pipeline/PSOManager.h"
#include "Player/TargetableComponent.h"
#include <cmath>
#include <fstream>
#include <nlohmann/json.hpp>
#include "Core/Utility/Log.h"
#include "Core/Utility/ContainerUtility.h"
#include "Core/Utility/JsonUtility.h"
#include <iostream>
#include <algorithm>
#include "Effects/EffectManagerComponent.h"
#include "Renderer/System/VoxelParticle/VoxelParticleManager.h"

void DebrisManagerComponent::OnRegisterProperties() {
    Component::OnRegisterProperties();
    RegisterProperty("Debris Pull Speed", &debrisPullSpeed_);
    RegisterProperty("Debris Throw Speed", &debrisThrowSpeed_);
    RegisterProperty("Debris Orbit Speed", &debrisOrbitSpeed_);
    RegisterProperty("Debris Orbit Radius", &debrisOrbitRadius_);
    RegisterProperty("Debris Damage (Boss)", &debrisDamage_);
    RegisterProperty("Debris Damage (Enemy)", &debrisEnemyDamage_);
    RegisterProperty("Debris Pull Y Offset", &debrisPullYOffset_);
    RegisterProperty("Camera Shake Intensity", &cameraShakeIntensity_);
    RegisterProperty("Camera Shake Duration(Frames)", &cameraShakeDurationFrames_);
    RegisterProperty("Player Aura Color", &playerAuraColor_);
    RegisterProperty("Boss Aura Color", &bossAuraColor_);
    RegisterProperty("Idle Aura Color", &idleAuraColor_);
    RegisterProperty("Catch Distance Sq", &catchDistanceSq_);
    RegisterProperty("Boss Shield Radius", &bossShieldRadius_);

    RegisterProperty("Debris Base Scale", &debrisBaseScale_);
    RegisterProperty("Debris Collider Radius", &colliderRadius_);
    RegisterProperty("Debris Aura Scale", &auraScale_);
    RegisterProperty("Max Throw Distance", &maxThrowDistance_);

    RegisterProperty("Auto Supply Enabled", &autoSupplyEnabled_);
    RegisterProperty("Target Field Count", &targetFieldCount_);
    RegisterProperty("Supply Check Interval", &supplyCheckInterval_);
    RegisterProperty("Recycle Behind Distance", &recycleBehindDistance_);
}

void DebrisManagerComponent::Initialize() {
    std::string configPath = "resources/GameData/DebrisPalette.json";
    nlohmann::json j;
    if (!Irufemi::JsonUtility::LoadFromFile(configPath, j)) {
        Log::OutPutLog(std::cout, "Failed to load DebrisPalette.json\n");
        return;
    }

    const auto& variationsJson = j["variations"];
    int varIndex = 0;
    for (const auto& v : variationsJson) {
        DebrisVariation var;
        var.id = v["id"].get<std::string>();
        var.modelPath = v["modelPath"].get<std::string>();
        var.maxVirtualCount = v["maxVirtualCount"].get<int>();
        var.maxPoolSize = v["maxPoolSize"].get<int>();
        var.spawnWeight = v["spawnWeight"].get<int>();

        var.poolObject = std::make_shared<GameObject>("DebrisPool_" + var.id);

        auto batchRenderer = var.poolObject->AddComponent<ModelBatchRendererComponent>();
        batchRenderer->LoadModel(var.modelPath);

        var.virtualManager = var.poolObject->AddComponent<VirtualEntityManagerComponent>().get();

        auto debrisFactory = [this, varIndex]() -> std::shared_ptr<GameObject> {
            auto obj = std::make_shared<GameObject>("Debris");
            auto transform = obj->GetTransform();
            transform->SetScale(debrisBaseScale_);

            auto debrisComp = obj->AddComponent<DebrisComponent>();
            debrisComp->SetVariationIndex(varIndex);

            obj->AddComponent<TargetableComponent>();

            auto collider = obj->AddComponent<SphereColliderComponent>();
            collider->isTrigger_ = true;
            collider->SetLocalRadius(colliderRadius_);

            // --- Aura (EnergyCore) ---
            auto aura = std::make_shared<GameObject>("DebrisAura");
            auto auraTransform = aura->GetTransform();
            auraTransform->SetScale(auraScale_);

            auto auraModel = aura->AddComponent<PrimitiveRendererComponent>();
            auraModel->Initialize();
            auraModel->SetShape(Irufemi::PrimitiveType::Sphere);

            if (auto primitive = static_cast<Primitive3DObject*>(auraModel->GetRenderable())) {
                primitive->SetCustomPSO("EnergyCore", Irufemi::BlendMode::kBlendModeAdd,
                                        PSOManager::DepthWrite::Disable, PSOManager::CullMode::Back);
                primitive->SetIsTransparent(true);
                primitive->SetColor(idleAuraColor_);
            }

            aura->SetIsActive(false);
            obj->AddChild(aura);
            // -------------------------

            obj->SetIsActive(false);
            return obj;
        };

        var.virtualManager->Setup(var.maxPoolSize, var.maxVirtualCount + 500, debrisFactory);
        var.animDataList.resize(var.maxVirtualCount + 500);

        if (gameObject_) {
            gameObject_->AddChild(var.poolObject);
        }

        variations_.emplace_back(std::move(var));
        varIndex++;
    }

    // ガレキ破砕用ボクセルモデルを事前確保（エンジン既存のReservePoolを呼び出してロード画面中に完了待機）
    if (auto scene = gameObject_ ? gameObject_->GetScene() : nullptr) {
        if (auto engine = scene->GetEngine()) {
            if (auto voxelMgr = engine->GetVoxelParticleManager()) {
                voxelMgr->ReservePool("resources/model/Debris/Generic/Debris_Generic.obj", {2, 2, 2}, 16);
            }
        }
    }
}

void DebrisManagerComponent::SpawnDebrisInFrontOfPlayer(int count) {
    if (count <= 0 || variations_.empty()) {
        return;
    }

    Irufemi::Vector3 spawnBase = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 forward = {0.0f, 0.0f, 1.0f};
    Irufemi::Vector3 right = {1.0f, 0.0f, 0.0f};

    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (scene) {
        auto playerObj = scene->FindGameObject("Player");
        if (playerObj) {
            if (auto t = playerObj->GetComponent<TransformComponent>()) {
                spawnBase = t->GetWorldPosition();
                float yaw = t->GetWorldRotation().y;
                forward = {std::sin(yaw), 0.0f, std::cos(yaw)};
                right = {std::cos(yaw), 0.0f, -std::sin(yaw)};
            }
        }
    }

    int totalWeight = 0;
    for (const auto& var : variations_) {
        totalWeight += var.spawnWeight;
    }
    if (totalWeight <= 0) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        float distFwd = (count > 100) ? Irufemi::Random::GeneratorFloat(10.0f, 300.0f)
                                      : Irufemi::Random::GeneratorFloat(30.0f, 85.0f);
        float distRight = (count > 100) ? Irufemi::Random::GeneratorFloat(-150.0f, 150.0f)
                                        : Irufemi::Random::GeneratorFloat(-14.0f, 14.0f);
        float height = (count > 100) ? Irufemi::Random::GeneratorFloat(-10.0f, 100.0f)
                                     : Irufemi::Random::GeneratorFloat(-2.0f, 8.0f);

        Irufemi::Vector3 pos = {spawnBase.x + forward.x * distFwd + right.x * distRight, spawnBase.y + height,
                                spawnBase.z + forward.z * distFwd + right.z * distRight};

        int randW = static_cast<int>(Irufemi::Random::GeneratorUint64(0, totalWeight - 1));
        int selectedIndex = 0;
        int currentW = 0;
        for (size_t v = 0; v < variations_.size(); ++v) {
            currentW += variations_[v].spawnWeight;
            if (randW <= currentW) {
                selectedIndex = static_cast<int>(v);
                break;
            }
        }

        auto& var = variations_[selectedIndex];
        if (!var.virtualManager) {
            continue;
        }

        int vid = var.virtualManager->AddVirtualInstance(pos, {0, 0, 0}, debrisBaseScale_);
        if (vid >= 0) {
            DebrisAnimData anim;
            anim.baseIdleY = pos.y;
            anim.idleTimeY = Irufemi::Random::GeneratorFloat(0.0f, 100.0f);
            var.animDataList[vid] = anim;
            var.activeIds.push(vid);
        }
    }

    for (auto& var : variations_) {
        if (!var.virtualManager) {
            continue;
        }
        while (var.activeIds.size() > static_cast<size_t>(var.maxVirtualCount)) {
            int oldestId = var.activeIds.front();
            var.activeIds.pop();
            var.virtualManager->RemoveVirtualInstance(oldestId);
        }
    }
}

void DebrisManagerComponent::SpawnDebrisCluster(const Irufemi::Vector3& centerPos, int count, float spreadRadius,
                                                const std::string& specificVariationId) {
    if (count <= 0 || variations_.empty()) {
        return;
    }

    int forcedIndex = -1;
    if (!specificVariationId.empty()) {
        for (size_t v = 0; v < variations_.size(); ++v) {
            if (variations_[v].id == specificVariationId) {
                forcedIndex = static_cast<int>(v);
                break;
            }
        }
    }

    int totalWeight = 0;
    if (forcedIndex < 0) {
        for (const auto& var : variations_) {
            totalWeight += var.spawnWeight;
        }
        if (totalWeight <= 0) {
            return;
        }
    }

    for (int i = 0; i < count; ++i) {
        float offsetX = Irufemi::Random::GeneratorFloat(-spreadRadius, spreadRadius);
        float offsetY = Irufemi::Random::GeneratorFloat(-spreadRadius * 0.5f, spreadRadius * 0.5f);
        float offsetZ = Irufemi::Random::GeneratorFloat(-spreadRadius, spreadRadius);

        Irufemi::Vector3 pos = {centerPos.x + offsetX, centerPos.y + offsetY, centerPos.z + offsetZ};

        int selectedIndex = forcedIndex;
        if (selectedIndex < 0) {
            int randW = static_cast<int>(Irufemi::Random::GeneratorUint64(0, totalWeight - 1));
            int currentW = 0;
            for (size_t v = 0; v < variations_.size(); ++v) {
                currentW += variations_[v].spawnWeight;
                if (randW <= currentW) {
                    selectedIndex = static_cast<int>(v);
                    break;
                }
            }
        }

        if (selectedIndex < 0 || selectedIndex >= static_cast<int>(variations_.size())) {
            continue;
        }

        auto& var = variations_[selectedIndex];
        if (!var.virtualManager) {
            continue;
        }

        int vid = var.virtualManager->AddVirtualInstance(pos, {0, 0, 0}, debrisBaseScale_);
        if (vid >= 0) {
            DebrisAnimData anim;
            anim.baseIdleY = pos.y;
            anim.idleTimeY = Irufemi::Random::GeneratorFloat(0.0f, 100.0f);
            var.animDataList[vid] = anim;
            var.activeIds.push(vid);
        }
    }

    for (auto& var : variations_) {
        if (!var.virtualManager) {
            continue;
        }
        while (var.activeIds.size() > static_cast<size_t>(var.maxVirtualCount)) {
            int oldestId = var.activeIds.front();
            var.activeIds.pop();
            var.virtualManager->RemoveVirtualInstance(oldestId);
        }
    }
}

void DebrisManagerComponent::SpawnDebrisBurst(const Irufemi::Vector3& origin, const Irufemi::Vector3& targetPlayerPos,
                                              int count, float spreadRadius, float forwardBias,
                                              const std::string& specificVariationId) {
    if (count <= 0 || variations_.empty()) {
        return;
    }

    Irufemi::Vector3 toPlayer = {targetPlayerPos.x - origin.x, targetPlayerPos.y - origin.y,
                                 targetPlayerPos.z - origin.z};
    float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);

    Irufemi::Vector3 burstCenter = origin;
    if (dist > 15.0f && forwardBias > 0.0f) {
        // 自機方向（手前）へ最大 forwardBias 分だけ寄せた位置を中心とする（引き寄せ有効射程内へ誘導）
        float biasDist = (std::min)(forwardBias, dist - 15.0f);
        Irufemi::Vector3 dir = {toPlayer.x / dist, toPlayer.y / dist, toPlayer.z / dist};
        burstCenter.x += dir.x * biasDist;
        burstCenter.y += dir.y * biasDist;
        burstCenter.z += dir.z * biasDist;
    }

    // 算出した中心座標でクラスターをスポーン
    SpawnDebrisCluster(burstCenter, count, spreadRadius, specificVariationId);
}

void DebrisManagerComponent::Update() {
    auto input = GetEngine() ? GetEngine()->GetInputManager() : nullptr;
    float deltaTime = GetEngine() ? GetEngine()->GetGameDeltaTime() : 0.0f;

    // --- ピラー1: 前方自動補充と後方リサイクル ---
    auto scene = gameObject_ ? gameObject_->GetScene() : nullptr;
    if (autoSupplyEnabled_ && scene && deltaTime > 0.0f) {
        supplyTimer_ += deltaTime;
        if (supplyTimer_ >= supplyCheckInterval_) {
            supplyTimer_ = 0.0f;

            auto playerObj = scene->FindGameObject("Player");
            if (playerObj) {
                if (auto pt = playerObj->GetComponent<TransformComponent>()) {
                    Irufemi::Vector3 playerPos = pt->GetWorldPosition();

                    // 1. 自機後方に外れたガレキをリサイクル
                    for (auto& var : variations_) {
                        if (!var.virtualManager) {
                            continue;
                        }
                        while (!var.activeIds.empty()) {
                            int oldestId = var.activeIds.front();
                            int sparseIdx = var.virtualManager->GetSparseIndex(oldestId);
                            if (sparseIdx >= 0 &&
                                sparseIdx < static_cast<int>(var.virtualManager->GetDenseInstances().size())) {
                                const auto& inst = var.virtualManager->GetDenseInstances()[sparseIdx];
                                if (inst.position.z < playerPos.z - recycleBehindDistance_) {
                                    var.activeIds.pop();
                                    var.virtualManager->RemoveVirtualInstance(oldestId);
                                    continue;
                                }
                            }
                            break;
                        }
                    }

                    // 2. 有効ガレキ数をカウントして不足分を補充
                    int activeCount = 0;
                    for (const auto& var : variations_) {
                        activeCount += static_cast<int>(var.activeIds.size());
                    }

                    if (activeCount < targetFieldCount_) {
                        int needed = targetFieldCount_ - activeCount;
                        SpawnDebrisInFrontOfPlayer((std::min)(needed, 6)); // スムーズに分散補充
                    }
                }
            }
        }
    }

    if (input) {
        if (input->IsKeyPressed('1') || input->IsKeyPressedDIK(0x02)) {
            SpawnDebrisInFrontOfPlayer(10);
        }
        if (input->IsKeyPressed('9') || input->IsKeyPressedDIK(0x0A)) {
            SpawnDebrisInFrontOfPlayer(10000);
        }
    }

    if (deltaTime > 0.0f) {
        UpdatePulledDebris(deltaTime);
        UpdateOrbitingDebris(deltaTime);
        UpdateBossOrbitingDebris(deltaTime);
        UpdateThrownDebris(deltaTime);
    }

    for (auto& debris : pendingReleases_) {
        ReleaseDebris(debris);
    }
    pendingReleases_.clear();

    for (auto& pair : pendingDestroys_) {
        NotifyDestroyed(pair.first, pair.second);
    }
    pendingDestroys_.clear();
}

std::shared_ptr<GameObject> DebrisManagerComponent::GetDebris() {
    if (variations_.empty()) {
        return nullptr;
    }

    // TODO: Boss戦等の動的取得用途において、専用PrefabIndexの指定に対応する（現在は0番の固定Prefabを使用）
    auto& var = variations_[0];
    int id = var.virtualManager->AddVirtualInstance({0, 0, 0}, {0, 0, 0}, {0.5f, 0.5f, 0.5f});
    auto obj = var.virtualManager->Promote(id);
    if (!obj) {
        if (DemoteFarthestIdleDebris({0.0f, 0.0f, 0.0f})) {
            obj = var.virtualManager->Promote(id);
        }
    }

    if (obj) {
        auto comp = obj->GetComponent<DebrisComponent>();
        if (comp) {
            comp->SetManager(this);
            comp->SetVirtualId(id);
            comp->SetVariationIndex(0);
            comp->ResetForPool();
            comp->SetState(DebrisState::Idle, true);
        }
    }
    return obj;
}

void DebrisManagerComponent::ReleaseDebris(std::shared_ptr<GameObject> debris) {
    if (!debris) {
        return;
    }
    auto comp = debris->GetComponent<DebrisComponent>();
    if (comp) {
        comp->ResetForPool();

        int vid = comp->GetVirtualId();
        int vIndex = comp->GetVariationIndex();
        if (vid >= 0 && vIndex >= 0 && vIndex < variations_.size()) {
            variations_[vIndex].virtualManager->Demote(vid);
        } else if (vIndex >= 0 && vIndex < variations_.size()) {
            variations_[vIndex].virtualManager->ReleaseGameObject(debris);
        } else {
            debris->SetIsActive(false);
        }
    }
}

void DebrisManagerComponent::MarkForRelease(std::shared_ptr<GameObject> debris) {
    if (debris) {
        pendingReleases_.push_back(debris);
    }
}

std::shared_ptr<GameObject> DebrisManagerComponent::ExtractNearestIdleDebris(const Irufemi::Vector3& pos,
                                                                             float radius) {
    float bestDistSq = radius * radius;
    std::shared_ptr<GameObject> bestObj = nullptr;

    // 1. 実体化されている Idle がれき（環境物破壊等で発生したもの）を最優先で探索
    // 無効になった要素を std::erase_if で一括クリーンアップ (O(N))
    std::erase_if(activeIdleDebris_, [](DebrisComponent* comp) {
        return !comp || !comp->GetGameObject() || !comp->GetGameObject()->GetIsActive();
    });

    for (auto* comp : activeIdleDebris_) {
        if (comp->GetState() == DebrisState::Idle) {
            if (auto t = comp->GetGameObject()->GetTransform()) {
                Irufemi::Vector3 d = t->GetWorldPosition() - pos;
                float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
                if (distSq <= bestDistSq) {
                    bestDistSq = distSq;
                    bestObj = comp->GetGameObject()->shared_from_this();
                }
            }
        }
    }

    if (bestObj) {
        return bestObj;
    }

    // 2. 実体が範囲内になければ、未実体化（!isPromoted_）の仮想インスタンスから最も近いものを探索
    int bestId = -1;
    int bestVarIndex = -1;

    for (size_t v = 0; v < variations_.size(); ++v) {
        auto& virtualInstances = variations_[v].virtualManager->GetDenseInstances();
        for (const auto& vi : virtualInstances) {
            if (!vi.isDestroyed && !vi.isPromoted) {
                float dx = vi.position.x - pos.x;
                float dy = vi.position.y - pos.y;
                float dz = vi.position.z - pos.z;
                float distSq = dx * dx + dy * dy + dz * dz;
                if (distSq <= bestDistSq) {
                    bestDistSq = distSq;
                    bestId = vi.id;
                    bestVarIndex = static_cast<int>(v);
                }
            }
        }
    }

    if (bestId >= 0 && bestVarIndex >= 0) {
        auto obj = variations_[bestVarIndex].virtualManager->Promote(bestId);
        if (!obj) {
            if (DemoteFarthestIdleDebris(pos)) {
                obj = variations_[bestVarIndex].virtualManager->Promote(bestId);
            }
        }
        if (obj) {
            auto comp = obj->GetComponent<DebrisComponent>();
            if (comp) {
                comp->SetVirtualId(bestId);
                comp->SetVariationIndex(bestVarIndex);
                comp->SetManager(this);
                comp->ResetForPool();
                comp->SetState(DebrisState::Idle, true);
            }
            return obj;
        }
    }
    return nullptr;
}

void DebrisManagerComponent::NotifyDestroyed(int virtualId, int variationIndex) {
    if (variationIndex >= 0 && variationIndex < variations_.size()) {
        variations_[variationIndex].virtualManager->RemoveVirtualInstance(virtualId);
    }
}

bool DebrisManagerComponent::DemoteFarthestIdleDebris(const Irufemi::Vector3& fromPos) {
    float maxDistSq = -1.0f;
    DebrisComponent* farthestComp = nullptr;

    for (auto* comp : activeIdleDebris_) {
        if (!comp || !comp->GetGameObject() || !comp->GetGameObject()->GetIsActive()) {
            continue;
        }
        if (comp->GetVirtualId() >= 0) {
            if (auto t = comp->GetGameObject()->GetTransform()) {
                Irufemi::Vector3 d = t->GetWorldPosition() - fromPos;
                float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
                if (distSq > maxDistSq) {
                    maxDistSq = distSq;
                    farthestComp = comp;
                }
            }
        }
    }

    if (farthestComp) {
        int vid = farthestComp->GetVirtualId();
        int vIndex = farthestComp->GetVariationIndex();
        if (vid >= 0 && vIndex >= 0 && vIndex < variations_.size()) {
            variations_[vIndex].virtualManager->Demote(vid);
            return true;
        }
    }
    return false;
}

void DebrisManagerComponent::MarkForDestroy(int virtualId, int variationIndex) {
    pendingDestroys_.push_back({virtualId, variationIndex});
}

void DebrisManagerComponent::RegisterDebris(DebrisComponent* debris, DebrisState state) {
    switch (state) {
    case DebrisState::Idle:
        activeIdleDebris_.push_back(debris);
        break;
    case DebrisState::Pulled:
        pulledDebris_.push_back(debris);
        break;
    case DebrisState::Orbiting:
        orbitingDebris_.push_back(debris);
        break;
    case DebrisState::BossOrbiting:
        bossOrbitingDebris_.push_back(debris);
        break;
    case DebrisState::Thrown:
        thrownDebris_.push_back(debris);
        break;
    default:
        break;
    }
}

void DebrisManagerComponent::UnregisterDebris(DebrisComponent* debris, DebrisState state) {
    auto remove_func = [debris](std::vector<DebrisComponent*>& vec) { Irufemi::Container::EraseSwap(vec, debris); };
    switch (state) {
    case DebrisState::Idle:
        remove_func(activeIdleDebris_);
        break;
    case DebrisState::Pulled:
        remove_func(pulledDebris_);
        break;
    case DebrisState::Orbiting:
        remove_func(orbitingDebris_);
        break;
    case DebrisState::BossOrbiting:
        remove_func(bossOrbitingDebris_);
        break;
    case DebrisState::Thrown:
        remove_func(thrownDebris_);
        break;
    default:
        break;
    }
}

void DebrisManagerComponent::UpdatePulledDebris(float deltaTime) {
    float catchDistSq = GetCatchDistanceSq();
    float pullSpeed = GetDebrisPullSpeed();
    float pullYOffset = GetDebrisPullYOffset();

    std::vector<DebrisComponent*> caughtDebris;

    for (int i = (int)pulledDebris_.size() - 1; i >= 0; --i) {
        DebrisComponent* debris = pulledDebris_[i];
        if (!debris || !debris->gameObject_) {
            continue;
        }
        auto transform = debris->GetTransform();
        if (!transform) {
            continue;
        }

        if (auto target = debris->targetObject_.lock()) {
            auto targetTransform = target->GetComponent<TransformComponent>();
            if (targetTransform) {
                Irufemi::Vector3 targetPos = targetTransform->GetWorldPosition();
                Irufemi::Vector3 pos = transform->GetWorldPosition();
                Irufemi::Vector3 diff = {targetPos.x - pos.x, targetPos.y + pullYOffset - pos.y, targetPos.z - pos.z};
                pos.x += diff.x * pullSpeed * deltaTime;
                pos.y += diff.y * pullSpeed * deltaTime;
                pos.z += diff.z * pullSpeed * deltaTime;
                transform->SetWorldPosition(pos);

                float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
                if (distSq < catchDistSq) {
                    caughtDebris.push_back(debris);
                }
            }
        }
    }

    for (auto* debris : caughtDebris) {
        debris->SetState(DebrisState::Orbiting);
    }
}

void DebrisManagerComponent::UpdateOrbitingDebris(float deltaTime) {
    float orbitSpeed = GetDebrisOrbitSpeed();

    for (int i = (int)orbitingDebris_.size() - 1; i >= 0; --i) {
        DebrisComponent* debris = orbitingDebris_[i];
        if (!debris || !debris->gameObject_) {
            continue;
        }
        auto transform = debris->GetTransform();
        if (!transform) {
            continue;
        }

        if (auto target = debris->targetObject_.lock()) {
            auto targetTransform = target->GetComponent<TransformComponent>();
            if (targetTransform) {
                debris->orbitAngle_ += orbitSpeed * deltaTime;

                Irufemi::Vector3 offset = {std::cos(debris->orbitAngle_) * debris->orbitRadius_,
                                           std::sin(debris->orbitAngle_ * 2.0f) * 0.5f + 1.0f,
                                           std::sin(debris->orbitAngle_) * debris->orbitRadius_};

                Irufemi::Vector3 pos = transform->GetWorldPosition();
                pos.x = targetTransform->GetWorldPosition().x + offset.x;
                pos.y = targetTransform->GetWorldPosition().y + offset.y;
                pos.z = targetTransform->GetWorldPosition().z + offset.z;
                transform->SetWorldPosition(pos);
            }
        }
    }
}

void DebrisManagerComponent::UpdateBossOrbitingDebris(float deltaTime) {
    float shieldRotationSpeed = 1.0f;
    float currentRadiusBase = GetBossShieldRadius();

    for (int i = (int)bossOrbitingDebris_.size() - 1; i >= 0; --i) {
        DebrisComponent* debris = bossOrbitingDebris_[i];
        if (!debris || !debris->gameObject_) {
            continue;
        }
        auto transform = debris->GetTransform();
        if (!transform) {
            continue;
        }

        if (auto target = debris->targetObject_.lock()) {
            auto targetTransform = target->GetComponent<TransformComponent>();
            if (targetTransform) {
                debris->bossOrbitAngleX_ += debris->bossOrbitSpeedX_ * shieldRotationSpeed * deltaTime;
                debris->bossOrbitAngleY_ += debris->bossOrbitSpeedY_ * shieldRotationSpeed * deltaTime;
                debris->bossOrbitAngleZ_ += debris->bossOrbitSpeedZ_ * shieldRotationSpeed * deltaTime;

                Irufemi::Matrix4x4 rotMatrix = Irufemi::Math::MakeRotateXYZMatrix(
                    Irufemi::Vector3{debris->bossOrbitAngleX_, debris->bossOrbitAngleY_, debris->bossOrbitAngleZ_});
                float currentRadius = currentRadiusBase + debris->bossOrbitRadiusOffset_;
                Irufemi::Vector3 baseOffset = {0, 0, currentRadius};
                Irufemi::Vector3 localPos = Irufemi::Math::TransformNormal(baseOffset, rotMatrix);

                Irufemi::Vector3 pos = transform->GetWorldPosition();
                pos.x = targetTransform->GetWorldPosition().x + localPos.x;
                pos.y = targetTransform->GetWorldPosition().y + localPos.y;
                pos.z = targetTransform->GetWorldPosition().z + localPos.z;
                transform->SetWorldPosition(pos);

                transform->SetRotation({debris->bossOrbitAngleX_ * 2.0f, debris->bossOrbitAngleY_ * 2.0f,
                                        debris->bossOrbitAngleZ_ * 2.0f});
            }
        }
    }
}

void DebrisManagerComponent::UpdateThrownDebris(float deltaTime) {
    float throwSpeed = GetDebrisThrowSpeed();
    float maxDistSq = GetMaxThrowDistanceSq();
    EffectManagerComponent* effectManager = nullptr;
    auto engine = GetEngine();
    auto voxelManager = engine ? engine->GetVoxelParticleManager() : nullptr;

    if (gameObject_ && gameObject_->GetScene()) {
        if (auto go = gameObject_->GetScene()->FindGameObject("EffectManager")) {
            effectManager = go->GetComponent<EffectManagerComponent>();
        }
    }

    for (int i = (int)thrownDebris_.size() - 1; i >= 0; --i) {
        DebrisComponent* debris = thrownDebris_[i];
        if (!debris || !debris->gameObject_) {
            continue;
        }
        auto transform = debris->GetTransform();
        if (!transform) {
            continue;
        }

        bool updatedWithTarget = false;
        if (auto target = debris->targetObject_.lock()) {
            if (target->GetIsActive()) {
                auto targetTransform = target->GetComponent<TransformComponent>();
                if (targetTransform) {
                    Irufemi::Vector3 targetPos = targetTransform->GetWorldPosition();
                    Irufemi::Vector3 pos = transform->GetWorldPosition();
                    Irufemi::Vector3 diff = {targetPos.x - pos.x, targetPos.y - pos.y, targetPos.z - pos.z};
                    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
                    float moveDist = throwSpeed * deltaTime;
                    // 到達判定（フレーム移動距離以内、または近接2.5m以内）
                    if (len <= (std::max)(moveDist, 2.5f)) {
                        pos = targetPos;
                        transform->SetWorldPosition(pos);

                        // ターゲットへのダメージ適用（シールドまたは敵本体）
                        if (auto debrisComp = target->GetComponent<DebrisComponent>()) {
                            if (debrisComp->GetState() == DebrisState::BossOrbiting) {
                                debrisComp->DestroyAsShield();
                            }
                        } else if (auto damageable = target->GetComponentByInterface<IDamageable>()) {
                            float damage = debris->GetEnemyDamage();
                            if (damageable->GetDamageableType() == DamageableType::Boss) {
                                damage = debris->GetBossDamage();
                            }
                            damageable->TakeDamage(damage);
                        }

                        // ヒット演出（エフェクト＋ボクセル破砕）
                        if (effectManager) {
                            effectManager->PlayEffect(debris->hitEffectKey_, pos);
                        }
                        if (voxelManager) {
                            VoxelEmitter p{};
                            p.particleType = 5; // DebrisExplosive
                            p.lifeTime = 1.0f;
                            p.gravity = 5.0f;
                            p.dispersion = 12.0f;
                            p.scale = {0.5f, 0.5f, 0.5f};
                            Irufemi::Vector4 aura = debris->GetPlayerAuraColor();
                            Irufemi::Vector4 rockColor = {1.5f, 1.2f, 1.0f, 1.0f};
                            p.startColor = {rockColor.x + aura.x * 2.0f, rockColor.y + aura.y * 2.0f,
                                            rockColor.z + aura.z * 2.0f, 1.0f};
                            p.endColor = {0.2f, 0.2f, 0.2f, 1.0f};
                            p.dissolveEdgeColor = aura;
                            voxelManager->PlayExplosion(debris->explosionModelPath_, pos, {0, 0, 0}, {0, 0, 0},
                                                        {1, 1, 1}, p, {2, 2, 2});
                        }

                        // ガレキをプールへ安全に返却
                        MarkForRelease(debris->gameObject_->shared_from_this());
                        if (debris->virtualId_ >= 0) {
                            MarkForDestroy(debris->virtualId_, debris->variationIndex_);
                        }
                        continue;
                    } else if (len > 0.001f) {
                        debris->throwDirection_ = {diff.x / len, diff.y / len, diff.z / len};
                    }
                    updatedWithTarget = true;
                }
            }
        }

        Irufemi::Vector3 pos = transform->GetWorldPosition();
        pos.x += debris->throwDirection_.x * throwSpeed * deltaTime;
        pos.y += debris->throwDirection_.y * throwSpeed * deltaTime;
        pos.z += debris->throwDirection_.z * throwSpeed * deltaTime;
        transform->SetWorldPosition(pos);

        float dx = pos.x - debris->throwOrigin_.x;
        float dy = pos.y - debris->throwOrigin_.y;
        float dz = pos.z - debris->throwOrigin_.z;
        float distSq = dx * dx + dy * dy + dz * dz;

        if (distSq > maxDistSq) {
            if (effectManager) {
                effectManager->PlayEffect(debris->hitEffectKey_, pos);
            }
            if (voxelManager) {
                VoxelEmitter p{};
                p.particleType = 5; // DebrisExplosive
                p.lifeTime = 1.0f;
                p.gravity = 5.0f;
                p.dispersion = 12.0f;
                p.scale = {0.5f, 0.5f, 0.5f};

                Irufemi::Vector4 aura =
                    (debris->state_ == DebrisState::BossOrbiting) ? GetBossAuraColor() : GetPlayerAuraColor();
                Irufemi::Vector4 rockColor = {1.5f, 1.2f, 1.0f, 1.0f};
                p.startColor = {rockColor.x + aura.x * 2.0f, rockColor.y + aura.y * 2.0f, rockColor.z + aura.z * 2.0f,
                                1.0f};
                p.endColor = {0.2f, 0.2f, 0.2f, 1.0f};
                p.dissolveEdgeColor = aura;

                voxelManager->PlayExplosion(debris->explosionModelPath_, pos, {0, 0, 0}, {0, 0, 0}, {1, 1, 1}, p,
                                            {2, 2, 2});
            }
            MarkForRelease(debris->gameObject_->shared_from_this());
        }
    }
}
