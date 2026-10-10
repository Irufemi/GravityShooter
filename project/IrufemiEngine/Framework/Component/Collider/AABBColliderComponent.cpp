#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Physics/CollisionManager.h"
#include "Core/Math/MathFunction.h"

AABBColliderComponent::AABBColliderComponent() {}

void AABBColliderComponent::DrawDebug() {}

void AABBColliderComponent::OnRegisterProperties() {
    ColliderComponent::OnRegisterProperties();
    RegisterProperty("Local Offset X", &localOffset_.x);
    RegisterProperty("Local Offset Y", &localOffset_.y);
    RegisterProperty("Local Offset Z", &localOffset_.z);
    RegisterProperty("Local Size X", &localSize_.x);
    RegisterProperty("Local Size Y", &localSize_.y);
    RegisterProperty("Local Size Z", &localSize_.z);
}

Irufemi::AABB AABBColliderComponent::GetWorldAABB() const {
    Irufemi::AABB aabb;
    auto* transform = GetTransform();
    if (transform) {
        Irufemi::Vector3 worldPos = transform->GetWorldPosition();
        Irufemi::Vector3 worldScale = transform->GetWorldScale();

        // オブジェクトの回転とスケールを考慮したワールド空間のローカルオフセット
        Irufemi::Vector3 worldOffset = transform->GetWorldRight() * (localOffset_.x * worldScale.x) +
                                       transform->GetWorldUp() * (localOffset_.y * worldScale.y) +
                                       transform->GetWorldForward() * (localOffset_.z * worldScale.z);

        Irufemi::Vector3 center = worldPos + worldOffset;

        // ローカル軸ごとのサイズベクトルをワールド空間に変換
        Irufemi::Vector3 rightSize = transform->GetWorldRight() * (localSize_.x * worldScale.x);
        Irufemi::Vector3 upSize = transform->GetWorldUp() * (localSize_.y * worldScale.y);
        Irufemi::Vector3 forwardSize = transform->GetWorldForward() * (localSize_.z * worldScale.z);

        // 各ワールド軸（X, Y, Z）への射影の絶対値の和がAABBのサイズ（extent）になる
        Irufemi::Vector3 extent;
        extent.x = std::abs(rightSize.x) + std::abs(upSize.x) + std::abs(forwardSize.x);
        extent.y = std::abs(rightSize.y) + std::abs(upSize.y) + std::abs(forwardSize.y);
        extent.z = std::abs(rightSize.z) + std::abs(upSize.z) + std::abs(forwardSize.z);

        aabb.min = {center.x - extent.x, center.y - extent.y, center.z - extent.z};
        aabb.max = {center.x + extent.x, center.y + extent.y, center.z + extent.z};
    } else {
        aabb.min = {localOffset_.x - localSize_.x, localOffset_.y - localSize_.y, localOffset_.z - localSize_.z};
        aabb.max = {localOffset_.x + localSize_.x, localOffset_.y + localSize_.y, localOffset_.z + localSize_.z};
    }
    return aabb;
}

nlohmann::json AABBColliderComponent::Serialize() {
    nlohmann::json j = ColliderComponent::Serialize();
    j["localOffset"] = {localOffset_.x, localOffset_.y, localOffset_.z};
    j["localSize"] = {localSize_.x, localSize_.y, localSize_.z};
    return j;
}

void AABBColliderComponent::Deserialize(const nlohmann::json& j) {
    ColliderComponent::Deserialize(j);
    if (j.contains("localOffset")) {
        localOffset_.x = j["localOffset"][0];
        localOffset_.y = j["localOffset"][1];
        localOffset_.z = j["localOffset"][2];
    } else if (j.contains("center")) {
        localOffset_.x = j["center"][0];
        localOffset_.y = j["center"][1];
        localOffset_.z = j["center"][2];
    }
    if (j.contains("localSize")) {
        localSize_.x = j["localSize"][0];
        localSize_.y = j["localSize"][1];
        localSize_.z = j["localSize"][2];
    } else if (j.contains("size")) {
        localSize_.x = j["size"][0];
        localSize_.y = j["size"][1];
        localSize_.z = j["size"][2];
    }
}

std::shared_ptr<Component> AABBColliderComponent::Clone() {
    auto clone = std::make_shared<AABBColliderComponent>();
    clone->CopyPropertiesFrom(this);
    clone->localOffset_ = this->localOffset_;
    clone->localSize_ = this->localSize_;
    return clone;
}

bool AABBColliderComponent::TestCollision(const ColliderComponent* other,
                                          Irufemi::Collision::CollisionResult& outResult) const {
    if (!other) {
        outResult.isHit = false;
        return false;
    }
    return other->TestCollisionWithAABB(this, outResult);
}

bool AABBColliderComponent::TestCollisionWithAABB(const AABBColliderComponent* aabb,
                                                  Irufemi::Collision::CollisionResult& outResult) const {
    if (!aabb) {
        outResult.isHit = false;
        return false;
    }
    outResult = Irufemi::Collision::GetCollisionResult(aabb->GetWorldAABB(), GetWorldAABB());
    return outResult.isHit;
}

bool AABBColliderComponent::TestCollisionWithSphere(const SphereColliderComponent* sphere,
                                                    Irufemi::Collision::CollisionResult& outResult) const {
    if (!sphere) {
        outResult.isHit = false;
        return false;
    }
    // sphere vs AABB (outResult normal は sphere を押し出す方向)
    outResult = Irufemi::Collision::GetCollisionResult(GetWorldAABB(), sphere->GetWorldSphere());
    outResult.normal = Irufemi::Math::Multiply(-1.0f, outResult.normal);
    return outResult.isHit;
}

bool AABBColliderComponent::TestCollisionWithOBB(const OBBColliderComponent* obb,
                                                 Irufemi::Collision::CollisionResult& outResult) const {
    if (!obb) {
        outResult.isHit = false;
        return false;
    }
    // obb vs AABB (outResult normal は AABB を押し出す方向)
    outResult = Irufemi::Collision::GetCollisionResult(obb->GetWorldOBB(), GetWorldAABB());
    outResult.normal = Irufemi::Math::Multiply(-1.0f, outResult.normal);
    return outResult.isHit;
}
