#include "Physics/CollisionManager.h"
#include "Core/Utility/Log.h"
#include "Core/Utility/ContainerUtility.h"
#include <iostream>
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Physics/Collision/Collision.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Renderer/Object/Line/LineClass.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include <algorithm>
#include "Core/Utility/JsonUtility.h"
#include "Core/Math/MathFunction.h"
#include "Core/System/ThreadPool.h"

void CollisionManager::Initialize(DebugPrimitiveRenderer* debugRenderer) {
    debugPrimitiveRenderer_ = debugRenderer;
    if (!debugLine_) {
        debugLine_ = std::make_unique<Line3DBatch>();
        debugLine_->Initialize();
    }

    layerNames_ = {"Default"};
    LoadLayers(layerConfigFilePath_);
}

CollisionManager::~CollisionManager() = default;

void CollisionManager::Clear() {
    std::unique_lock<std::shared_mutex> lock(collidersMutex_);
    colliders_.clear();
    previousCollisions_.clear();
    dynamicBVH_.Clear();

    std::lock_guard<std::mutex> pendingLock(pendingMutex_);
    pendingAdds_.clear();
    pendingRemoves_.clear();
}

void CollisionManager::RegisterCollider(ColliderComponent* collider) {
    if (!collider) {
        return;
    }
    std::lock_guard<std::mutex> lock(pendingMutex_);
    // 重複を避けて末尾に追加
    Irufemi::Container::PushBackUnique(pendingAdds_, collider);
}

void CollisionManager::UnregisterCollider(ColliderComponent* collider) {
    if (!collider) {
        return;
    }
    std::lock_guard<std::mutex> lock(pendingMutex_);
    auto it = std::find_if(pendingRemoves_.begin(), pendingRemoves_.end(),
                           [collider](const PendingRemove& pr) { return pr.collider == collider; });
    if (it == pendingRemoves_.end()) {
        pendingRemoves_.push_back({collider, collider->bvhNodeId_});
    }
}

void CollisionManager::FlushPendingCommands() {
    std::vector<ColliderComponent*> adds;
    std::vector<PendingRemove> removes;

    {
        std::lock_guard<std::mutex> pendingLock(pendingMutex_);
        adds = std::move(pendingAdds_);
        removes = std::move(pendingRemoves_);
    }

    if (adds.empty() && removes.empty()) {
        return;
    }

    std::unique_lock<std::shared_mutex> lock(collidersMutex_);

    // 削除の適用
    for (const PendingRemove& removeInfo : removes) {
        ColliderComponent* collider = removeInfo.collider;
        if (Irufemi::Container::EraseSwap(colliders_, collider)) {
            if (removeInfo.bvhNodeId != -1) {
                dynamicBVH_.Remove(removeInfo.bvhNodeId);
                // Note: We cannot set collider->bvhNodeId_ = -1 here because collider might be destroyed
            }
        }

        for (auto iter = previousCollisions_.begin(); iter != previousCollisions_.end();) {
            if (iter->first == collider || iter->second == collider) {
                ColliderComponent* other = (iter->first == collider) ? iter->second : iter->first;
                // Note: collider might already be destroyed, so do not call collider->GetGameObject() or pass dead
                // pointer
                if (other && other->onCollisionExit_) {
                    other->onCollisionExit_(nullptr);
                }
                if (other && other->GetGameObject()) {
                    other->GetGameObject()->SendCollisionExit(nullptr);
                }
                iter = previousCollisions_.erase(iter);
            } else {
                ++iter;
            }
        }
    }

    // 追加の適用
    for (ColliderComponent* collider : adds) {
        if (!collider) {
            continue;
        }
        auto it = std::find(colliders_.begin(), colliders_.end(), collider);
        if (it == colliders_.end()) {
            colliders_.push_back(collider);
            collider->bvhNodeId_ = dynamicBVH_.Insert(collider, collider->GetBoundingBox());
        }
    }
}

void CollisionManager::CheckNarrowPhase(ColliderComponent* colA, ColliderComponent* colB,
                                        Irufemi::Collision::CollisionResult& outResult) const {
    outResult.isHit = false;
    if (!colA || !colB) {
        return;
    }

    auto typeA = colA->GetColliderType();
    auto typeB = colB->GetColliderType();

    if (typeA == ColliderComponent::ColliderType::AABB) {
        Irufemi::AABB boxA = static_cast<AABBColliderComponent*>(colA)->GetWorldAABB();

        if (typeB == ColliderComponent::ColliderType::AABB) {
            Irufemi::AABB boxB = static_cast<AABBColliderComponent*>(colB)->GetWorldAABB();
            outResult = Irufemi::Collision::GetCollisionResult(boxA, boxB);
        } else if (typeB == ColliderComponent::ColliderType::Sphere) {
            Irufemi::Sphere sphereB = static_cast<SphereColliderComponent*>(colB)->GetWorldSphere();
            outResult = Irufemi::Collision::GetCollisionResult(boxA, sphereB);
        } else if (typeB == ColliderComponent::ColliderType::OBB) {
            Irufemi::OBB obbB = static_cast<OBBColliderComponent*>(colB)->GetWorldOBB();
            outResult = Irufemi::Collision::GetCollisionResult(obbB, boxA);      // OBB vs AABB
            outResult.normal = Irufemi::Math::Multiply(-1.0f, outResult.normal); // OBBを押し出す方向の逆にする
        }
    } else if (typeA == ColliderComponent::ColliderType::Sphere) {
        Irufemi::Sphere sphereA = static_cast<SphereColliderComponent*>(colA)->GetWorldSphere();

        if (typeB == ColliderComponent::ColliderType::AABB) {
            Irufemi::AABB boxB = static_cast<AABBColliderComponent*>(colB)->GetWorldAABB();
            outResult = Irufemi::Collision::GetCollisionResult(boxB, sphereA);
            outResult.normal = Irufemi::Math::Multiply(-1.0f, outResult.normal);
        } else if (typeB == ColliderComponent::ColliderType::Sphere) {
            Irufemi::Sphere sphereB = static_cast<SphereColliderComponent*>(colB)->GetWorldSphere();
            outResult = Irufemi::Collision::GetCollisionResult(sphereA, sphereB);
        } else if (typeB == ColliderComponent::ColliderType::OBB) {
            Irufemi::OBB obbB = static_cast<OBBColliderComponent*>(colB)->GetWorldOBB();
            outResult = Irufemi::Collision::GetCollisionResult(obbB, sphereA);
            outResult.normal = Irufemi::Math::Multiply(-1.0f, outResult.normal);
        }
    } else if (typeA == ColliderComponent::ColliderType::OBB) {
        Irufemi::OBB obbA = static_cast<OBBColliderComponent*>(colA)->GetWorldOBB();

        if (typeB == ColliderComponent::ColliderType::AABB) {
            Irufemi::AABB boxB = static_cast<AABBColliderComponent*>(colB)->GetWorldAABB();
            outResult = Irufemi::Collision::GetCollisionResult(obbA, boxB);
        } else if (typeB == ColliderComponent::ColliderType::Sphere) {
            Irufemi::Sphere sphereB = static_cast<SphereColliderComponent*>(colB)->GetWorldSphere();
            outResult = Irufemi::Collision::GetCollisionResult(obbA, sphereB);
        } else if (typeB == ColliderComponent::ColliderType::OBB) {
            Irufemi::OBB obbB = static_cast<OBBColliderComponent*>(colB)->GetWorldOBB();
            outResult = Irufemi::Collision::GetCollisionResult(obbA, obbB);
        }
    }
}

void CollisionManager::DispatchCollisionEvents(ColliderComponent* colA, ColliderComponent* colB,
                                               const Irufemi::Collision::CollisionResult& result, bool isNewHit) {
    if (!colA || !colB || !result.isHit) {
        return;
    }

    if (isNewHit) {
        if (colA->onCollisionEnter_) {
            colA->onCollisionEnter_(colB);
        }
        if (colB->onCollisionEnter_) {
            colB->onCollisionEnter_(colA);
        }

        auto goA = colA->GetGameObject();
        auto goB = colB->GetGameObject();

        if (goA && goA->GetIsActive()) {
            goA->SendCollisionEnter(goB);
        }
        if (goB && goB->GetIsActive() && goA && goA->GetIsActive()) {
            goB->SendCollisionEnter(goA);
        }
    } else {
        if (colA->onCollisionStay_) {
            colA->onCollisionStay_(colB);
        }
        if (colB->onCollisionStay_) {
            colB->onCollisionStay_(colA);
        }

        auto goA = colA->GetGameObject();
        auto goB = colB->GetGameObject();

        if (goA && goA->GetIsActive()) {
            goA->SendCollisionStay(goB);
        }
        if (goB && goB->GetIsActive() && goA && goA->GetIsActive()) {
            goB->SendCollisionStay(goA);
        }
    }
}

void CollisionManager::ResolveKinematicCollision(ColliderComponent* colA, ColliderComponent* colB,
                                                 const Irufemi::Collision::CollisionResult& result) {
    if (!colA || !colB || colA->isTrigger_ || colB->isTrigger_) {
        return;
    }

    TransformComponent* transformA =
        colA->GetGameObject() ? colA->GetGameObject()->GetComponent<TransformComponent>() : nullptr;
    TransformComponent* transformB =
        colB->GetGameObject() ? colB->GetGameObject()->GetComponent<TransformComponent>() : nullptr;

    bool canMoveA = transformA && !colA->isStatic_;
    bool canMoveB = transformB && !colB->isStatic_;

    Irufemi::Vector3 pushA = {0.0f, 0.0f, 0.0f};
    Irufemi::Vector3 pushB = {0.0f, 0.0f, 0.0f};

    if (canMoveA && canMoveB) {
        // 両方動く場合は半分の距離ずつ押し戻す
        pushA = Irufemi::Math::Multiply(result.depth * 0.5f, result.normal);
        pushB = Irufemi::Math::Multiply(result.depth * 0.5f, Irufemi::Math::Multiply(-1.0f, result.normal));
    } else if (canMoveA) {
        pushA = Irufemi::Math::Multiply(result.depth, result.normal);
    } else if (canMoveB) {
        pushB = Irufemi::Math::Multiply(result.depth, Irufemi::Math::Multiply(-1.0f, result.normal));
    }

    if (canMoveA) {
        pushA = pushA * colA->pushbackMask_;
        transformA->SetWorldPosition(Irufemi::Math::Add(transformA->GetWorldPosition(), pushA));
    }
    if (canMoveB) {
        pushB = pushB * colB->pushbackMask_;
        transformB->SetWorldPosition(Irufemi::Math::Add(transformB->GetWorldPosition(), pushB));
    }
}

void CollisionManager::CheckAllCollisions() {
    FlushPendingCommands();

    currentCollisions_.clear();

    // --- BVH Update Phase ---
    {
        // BVHの更新はツリー構造の変更を伴うため、排他ロック（Unique Lock）を取得する
        std::unique_lock<std::shared_mutex> lock(collidersMutex_);
        for (ColliderComponent* collider : colliders_) {
            if (!collider) {
                continue;
            }
            auto go = collider->GetGameObject();
            if (!go || !go->GetIsActive()) {
                continue;
            }
            dynamicBVH_.Update(collider->bvhNodeId_, collider->GetBoundingBox());
        }
    }

    // --- Broad Phase & Narrow Phase & Resolution ---
    // BVHの更新が終わったため、判定自体はリードロック（Shared Lock）で行う
    std::shared_lock<std::shared_mutex> sharedLock(collidersMutex_);
    std::vector<ColliderComponent*> potentialHits;

    for (size_t i = 0; i < colliders_.size(); ++i) {
        ColliderComponent* colA = colliders_[i];
        if (!colA) {
            continue;
        }
        auto goA = colA->GetGameObject();
        if (!goA || !goA->GetIsActive()) {
            continue;
        }

        potentialHits.clear();
        dynamicBVH_.Query(colA->GetBoundingBox(), potentialHits);

        for (ColliderComponent* colB : potentialHits) {
            if (!colB || colA == colB) {
                continue;
            }
            auto goB = colB->GetGameObject();
            if (!goB || !goB->GetIsActive()) {
                continue;
            }

            // 重複判定を防ぐため、アドレスが小さい方から大きい方へのみ判定を行う
            if (colA >= colB) {
                continue;
            }

            // フィルタリング:
            // どちらかが Trigger の場合は、相手をマスクしている側がいれば検知可能とする
            bool colAHitsB = (colA->mask_ & colB->layer_) != 0;
            bool colBHitsA = (colB->mask_ & colA->layer_) != 0;

            if (colA->isTrigger_ || colB->isTrigger_) {
                if (!colAHitsB && !colBHitsA) {
                    continue;
                }
            } else {
                if (!colAHitsB || !colBHitsA) {
                    continue;
                }
            }

            // 静的オブジェクト同士の判定は不要（動かないため、計算負荷を削減）
            if (colA->isStatic_ && colB->isStatic_) {
                continue;
            }

            // アドレスでソートしてペアを作成 (colA < colB is guaranteed)
            auto pairKey = std::make_pair(colA, colB);

            // --- Narrow Phase (判定ディスパッチ) ---
            Irufemi::Collision::CollisionResult result;
            CheckNarrowPhase(colA, colB, result);

            if (result.isHit) {
                currentCollisions_.insert(pairKey);

                // --- コールバック呼び出し (Enter / Stay) ---
                bool isNewHit = (previousCollisions_.find(pairKey) == previousCollisions_.end());
                DispatchCollisionEvents(colA, colB, result, isNewHit);

                // --- 押し戻し処理 (Kinematic Resolution) ---
                ResolveKinematicCollision(colA, colB, result);
            }
        }
    }

    // --- 離脱処理 (Exit) ---
    for (const auto& pair : previousCollisions_) {
        // 前フレームでは当たっていたが、今フレームでは当たっていない
        if (currentCollisions_.find(pair) == currentCollisions_.end()) {
            ColliderComponent* colA = pair.first;
            ColliderComponent* colB = pair.second;

            if (colA && colA->onCollisionExit_) {
                colA->onCollisionExit_(colB);
            }
            if (colB && colB->onCollisionExit_) {
                colB->onCollisionExit_(colA);
            }

            if (colA && colA->GetGameObject()) {
                colA->GetGameObject()->SendCollisionExit(colB ? colB->GetGameObject() : nullptr);
            }
            if (colB && colB->GetGameObject()) {
                colB->GetGameObject()->SendCollisionExit(colA ? colA->GetGameObject() : nullptr);
            }
        }
    }

    // 更新（Ping-Pong スワップによりヒープ確保なしでバッファ再利用）
    std::swap(previousCollisions_, currentCollisions_);
}
void CollisionManager::DrawDebug(GameObject* selectedObject) {
    // 保留中のコライダー追加・削除コマンドを確定してデバッグ描画に即時反映
    FlushPendingCommands();

    if (!debugLine_) {
        return;
    }

    debugLine_->ClearInstances();

    debugRemoveList_.clear();
    {
        std::lock_guard<std::mutex> pendingLock(pendingMutex_);
        for (const auto& pr : pendingRemoves_) {
            debugRemoveList_.push_back(pr.collider);
        }
    }

    std::shared_lock<std::shared_mutex> collidersLock(collidersMutex_);
    for (ColliderComponent* collider : colliders_) {
        if (!collider) {
            continue;
        }
        if (std::find(debugRemoveList_.begin(), debugRemoveList_.end(), collider) != debugRemoveList_.end()) {
            continue;
        }
        auto go = collider->GetGameObject();
        if (!go) {
            continue;
        }
        if (reinterpret_cast<uintptr_t>(go) < 0x1000) {
            Log::OutPutLog(std::cerr, "[CollisionManager] CRITICAL ERROR: Caught invalid GameObject pointer (0x" +
                                          std::format("{:X}", reinterpret_cast<uintptr_t>(go)) +
                                          ") in CollisionManager!\n");
            continue;
        }
        if (!go->GetIsActive()) {
            continue;
        }

        bool isSelected = false;
        if (selectedObject) {
            GameObject* colObj = collider->GetGameObject();
            GameObject* selObj = selectedObject;

            // コライダーの持ち主が、選択されたオブジェクトの親（または同一）か？
            while (selObj) {
                if (colObj == selObj) {
                    isSelected = true;
                    break;
                }
                selObj = selObj->GetParent().get();
            }

            // 逆に、コライダーの持ち主が、選択されたオブジェクトの子孫か？
            if (!isSelected) {
                while (colObj) {
                    if (colObj == selectedObject) {
                        isSelected = true;
                        break;
                    }
                    colObj = colObj->GetParent().get();
                }
            }
        }

        // 全体表示OFFのときでも、選択中のオブジェクト（またはその親・子）のコライダーは表示する
        if (!isDrawDebugLine_ && !isSelected) {
            continue;
        }

        DebugCategory category = collider->GetDebugCategory();
        Irufemi::Vector4 color =
            isSelected ? Irufemi::Vector4{1.0f, 0.5f, 0.0f, 1.0f}
                       : collider->GetDebugCustomColor().value_or(Irufemi::Vector4{0.0f, 1.0f, 0.0f, 1.0f});

        if (collider->GetColliderType() == ColliderComponent::ColliderType::AABB) {
            AABBColliderComponent* aabbCol = static_cast<AABBColliderComponent*>(collider);
            Irufemi::AABB aabb = aabbCol->GetWorldAABB();

            Irufemi::Vector3 size = {aabb.max.x - aabb.min.x, aabb.max.y - aabb.min.y, aabb.max.z - aabb.min.z};
            Irufemi::Vector3 center = {(aabb.max.x + aabb.min.x) * 0.5f, (aabb.max.y + aabb.min.y) * 0.5f,
                                       (aabb.max.z + aabb.min.z) * 0.5f};
            Irufemi::Matrix4x4 transform = Irufemi::Math::MakeAffineMatrix(size, Irufemi::Vector3{0, 0, 0}, center);

            if (debugPrimitiveRenderer_) {
                debugPrimitiveRenderer_->AddCube(transform, color, category);
            }
        } else if (collider->GetColliderType() == ColliderComponent::ColliderType::Sphere) {
            SphereColliderComponent* sphereCol = static_cast<SphereColliderComponent*>(collider);
            Irufemi::Sphere sphere = sphereCol->GetWorldSphere();

            if (debugPrimitiveRenderer_) {
                debugPrimitiveRenderer_->AddSphere(sphere.center, sphere.radius, color, category);
            }
        } else if (collider->GetColliderType() == ColliderComponent::ColliderType::OBB) {
            OBBColliderComponent* obbCol = static_cast<OBBColliderComponent*>(collider);
            Irufemi::OBB obb = obbCol->GetWorldOBB();

            Irufemi::Matrix4x4 transform;
            transform.m[0][0] = obb.orientations[0].x * obb.size.x;
            transform.m[0][1] = obb.orientations[0].y * obb.size.x;
            transform.m[0][2] = obb.orientations[0].z * obb.size.x;
            transform.m[0][3] = 0.0f;
            transform.m[1][0] = obb.orientations[1].x * obb.size.y;
            transform.m[1][1] = obb.orientations[1].y * obb.size.y;
            transform.m[1][2] = obb.orientations[1].z * obb.size.y;
            transform.m[1][3] = 0.0f;
            transform.m[2][0] = obb.orientations[2].x * obb.size.z;
            transform.m[2][1] = obb.orientations[2].y * obb.size.z;
            transform.m[2][2] = obb.orientations[2].z * obb.size.z;
            transform.m[2][3] = 0.0f;
            transform.m[3][0] = obb.center.x;
            transform.m[3][1] = obb.center.y;
            transform.m[3][2] = obb.center.z;
            transform.m[3][3] = 1.0f;

            if (debugPrimitiveRenderer_) {
                debugPrimitiveRenderer_->AddCube(transform, color, category);
            }
        }
    } // end for colliders_

    // Raycastのデバッグ描画（コライダーの描画フラグとは独立して描画）
    for (const auto& r : debugRays_) {
        Irufemi::Vector3 dir = Irufemi::Math::Normalize(r.ray.diff);
        float drawDist = r.distance > 1000.0f ? 1000.0f : r.distance;
        Irufemi::Vector3 endPoint = r.ray.origin + dir * drawDist;
        debugLine_->AddInstance(r.ray.origin, endPoint, r.color);
    }
    debugRays_.clear();

    debugLine_->Update();
    debugLine_->Draw();
}

void CollisionManager::LoadLayers(const std::string& filepath) {
    nlohmann::json j;
    if (Irufemi::JsonUtility::LoadFromFile(filepath, j)) {
        if (j.contains("layers") && j["layers"].is_array()) {
            layerNames_.clear();
            for (const auto& name : j["layers"]) {
                layerNames_.push_back(name);
            }
        }
    }
}

void CollisionManager::SaveLayers(const std::string& filepath) {
    nlohmann::json j;
    j["layers"] = layerNames_;
    Irufemi::JsonUtility::SaveToFile(filepath, j, 4);
}

void CollisionManager::AddLayer(const std::string& name) {
    if (layerNames_.size() < 32) {
        layerNames_.push_back(name);
        SaveLayers(layerConfigFilePath_);
    }
}

void CollisionManager::RemoveLayer(int index) {
    if (index > 0 && index < layerNames_.size()) { // Default(index=0)は消せないようにする
        layerNames_.erase(layerNames_.begin() + index);
        SaveLayers(layerConfigFilePath_);
    }
}

void CollisionManager::RenameLayer(int index, const std::string& name) {
    if (index > 0 && index < layerNames_.size()) {
        layerNames_[index] = name;
        SaveLayers(layerConfigFilePath_);
    }
}

uint32_t CollisionManager::GetLayerMask(const std::string& name) const {
    for (size_t i = 0; i < layerNames_.size(); ++i) {
        if (layerNames_[i] == name) {
            return 1 << i;
        }
    }
    return 0; // Not found
}

bool CollisionManager::Raycast(const Irufemi::Ray& ray, RaycastHit& hitInfo, float maxDistance, uint32_t layerMask,
                               GameObject* ignoreObject) {
    hitInfo.isHit = false;
    hitInfo.distance = maxDistance;

    std::shared_lock<std::shared_mutex> lock(collidersMutex_);

    std::vector<ColliderComponent*> potentialHits;
    dynamicBVH_.RaycastQuery(ray, maxDistance, potentialHits);

    for (ColliderComponent* collider : potentialHits) {
        if (!collider) {
            continue;
        }
        auto go = collider->GetGameObject();
        if (!go) {
            continue;
        }
        if (reinterpret_cast<uintptr_t>(go) < 0x1000) {
            Log::OutPutLog(std::cerr, "[CollisionManager] CRITICAL ERROR: Caught invalid GameObject pointer (0x" +
                                          std::format("{:X}", reinterpret_cast<uintptr_t>(go)) +
                                          ") in CollisionManager!\n");
            continue;
        }
        if (!go->GetIsActive()) {
            continue;
        }

        // 除外オブジェクトならスキップ
        if (ignoreObject && collider->GetGameObject() == ignoreObject) {
            continue;
        }

        // 指定されたレイヤーマスクに合致するか判定
        if ((collider->layer_ & layerMask) == 0) {
            continue;
        }

        float distance = 0.0f;
        bool isHit = false;

        switch (collider->GetColliderType()) {
        case ColliderComponent::ColliderType::AABB: {
            AABBColliderComponent* aabbCol = static_cast<AABBColliderComponent*>(collider);
            isHit = Irufemi::Collision::IsCollision(ray, aabbCol->GetWorldAABB(), distance);
            break;
        }
        case ColliderComponent::ColliderType::Sphere: {
            SphereColliderComponent* sphereCol = static_cast<SphereColliderComponent*>(collider);
            isHit = Irufemi::Collision::IsCollision(ray, sphereCol->GetWorldSphere(), distance);
            break;
        }
        case ColliderComponent::ColliderType::OBB: {
            OBBColliderComponent* obbCol = static_cast<OBBColliderComponent*>(collider);
            isHit = Irufemi::Collision::IsCollision(ray, obbCol->GetWorldOBB(), distance);
            break;
        }
        }

        if (isHit && distance < hitInfo.distance) {
            hitInfo.isHit = true;
            hitInfo.distance = distance;
            hitInfo.hitCollider = collider;
            hitInfo.hitObject = collider->GetGameObject();
            hitInfo.hitPoint = ray.origin + Irufemi::Math::Normalize(ray.diff) * distance;
        }
    }

    return hitInfo.isHit;
}

void CollisionManager::QueryAABB(const Irufemi::AABB& aabb, std::vector<ColliderComponent*>& outHits) const {
    std::shared_lock<std::shared_mutex> lock(collidersMutex_);
    dynamicBVH_.Query(aabb, outHits);
}

void CollisionManager::DrawDebugRay(const Irufemi::Ray& ray, float distance, const Irufemi::Vector4& color) {
    debugRays_.push_back({ray, distance, color});
}

void CollisionManager::DrawDebugAABB(const Irufemi::AABB& aabb, const Irufemi::Vector4& color, DebugCategory category) {
    if (debugPrimitiveRenderer_) {
        Irufemi::Vector3 center = (aabb.min + aabb.max) * 0.5f;
        Irufemi::Vector3 size = aabb.max - aabb.min;
        Irufemi::Matrix4x4 transform = Irufemi::Math::MakeAffineMatrix(size, Irufemi::Vector3::zero, center);
        debugPrimitiveRenderer_->AddCube(transform, color, category);
    }
}

std::future<std::pair<bool, RaycastHit>> CollisionManager::RaycastAsync(ThreadPool* pool, const Irufemi::Ray& ray,
                                                                        float maxDistance, uint32_t layerMask,
                                                                        GameObject* ignoreObject) {
    if (!pool) {
        std::promise<std::pair<bool, RaycastHit>> prom;
        RaycastHit hit;
        bool result = Raycast(ray, hit, maxDistance, layerMask, ignoreObject);
        prom.set_value({result, hit});
        return prom.get_future();
    }

    return pool->Enqueue([this, ray, maxDistance, layerMask, ignoreObject]() -> std::pair<bool, RaycastHit> {
        RaycastHit hit;
        bool result = this->Raycast(ray, hit, maxDistance, layerMask, ignoreObject);
        return {result, hit};
    });
}
