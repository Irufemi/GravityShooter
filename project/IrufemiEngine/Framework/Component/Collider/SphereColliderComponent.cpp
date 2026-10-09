#include "Framework/Component/Collider/SphereColliderComponent.h"
#include "Framework/Component/Collider/AABBColliderComponent.h"
#include "Framework/Component/Collider/OBBColliderComponent.h"
#include "Framework/GameObject/GameObject.h"
#include "Framework/Component/TransformComponent.h"
#include "Physics/CollisionManager.h"
#include "Core/Math/MathFunction.h"

#include <algorithm>
#include <cmath>

SphereColliderComponent::SphereColliderComponent() {}

void SphereColliderComponent::OnRegisterProperties() {
    ColliderComponent::OnRegisterProperties();
    RegisterProperty("Local Offset", &localOffset_);
    RegisterProperty("Local Radius", &localRadius_);
    // TODO: 必要に応じてシリアライズ対象にLayerおよびMaskプロパティを追加する
}

void SphereColliderComponent::DrawDebug() {}

Irufemi::Sphere SphereColliderComponent::GetWorldSphere() const {
    Irufemi::Sphere sphere;
    auto* transform = GetTransform();
    if (transform) {
        Irufemi::Vector3 worldPos = transform->GetWorldPosition();
        Irufemi::Vector3 worldScale = transform->GetWorldScale();

        // スケールの最大成分を半径に掛ける
        float scaleX = std::abs(worldScale.x);
        float scaleY = std::abs(worldScale.y);
        float scaleZ = std::abs(worldScale.z);

        // オブジェクトの回転とスケールを考慮したワールド空間のローカルオフセット
        Irufemi::Vector3 worldOffset = transform->GetWorldRight() * (localOffset_.x * scaleX) +
                                       transform->GetWorldUp() * (localOffset_.y * scaleY) +
                                       transform->GetWorldForward() * (localOffset_.z * scaleZ);

        float maxScale = (std::max)({scaleX, scaleY, scaleZ});

        sphere.center = worldPos + worldOffset;
        sphere.radius = localRadius_ * maxScale;
    } else {
        sphere.center = localOffset_;
        sphere.radius = localRadius_;
    }
    return sphere;
}

Irufemi::AABB SphereColliderComponent::GetBoundingBox() const {
    Irufemi::Sphere sphere = GetWorldSphere();
    Irufemi::AABB aabb;
    aabb.min = {sphere.center.x - sphere.radius, sphere.center.y - sphere.radius, sphere.center.z - sphere.radius};
    aabb.max = {sphere.center.x + sphere.radius, sphere.center.y + sphere.radius, sphere.center.z + sphere.radius};
    return aabb;
}

nlohmann::json SphereColliderComponent::Serialize() {
    nlohmann::json j = ColliderComponent::Serialize();
    j["localOffset"] = {localOffset_.x, localOffset_.y, localOffset_.z};
    j["localRadius"] = localRadius_;
    return j;
}

void SphereColliderComponent::Deserialize(const nlohmann::json& j) {
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
    if (j.contains("localRadius")) {
        localRadius_ = j["localRadius"];
    } else if (j.contains("radius")) {
        localRadius_ = j["radius"];
    }
}

std::shared_ptr<Component> SphereColliderComponent::Clone() {
    auto clone = std::make_shared<SphereColliderComponent>();
    clone->CopyPropertiesFrom(this);
    clone->localOffset_ = this->localOffset_;
    clone->localRadius_ = this->localRadius_;
    return clone;
}

bool SphereColliderComponent::TestCollision(const ColliderComponent* other,
                                            Irufemi::Collision::CollisionResult& outResult) const {
    if (!other) {
        outResult.isHit = false;
        return false;
    }
    return other->TestCollisionWithSphere(this, outResult);
}

bool SphereColliderComponent::TestCollisionWithAABB(const AABBColliderComponent* aabb,
                                                    Irufemi::Collision::CollisionResult& outResult) const {
    if (!aabb) {
        outResult.isHit = false;
        return false;
    }
    // aabb vs Sphere (outResult normal は Sphere を押し出す方向)
    outResult = Irufemi::Collision::GetCollisionResult(aabb->GetWorldAABB(), GetWorldSphere());
    return outResult.isHit;
}

bool SphereColliderComponent::TestCollisionWithSphere(const SphereColliderComponent* sphere,
                                                      Irufemi::Collision::CollisionResult& outResult) const {
    if (!sphere) {
        outResult.isHit = false;
        return false;
    }
    outResult = Irufemi::Collision::GetCollisionResult(sphere->GetWorldSphere(), GetWorldSphere());
    return outResult.isHit;
}

bool SphereColliderComponent::TestCollisionWithOBB(const OBBColliderComponent* obb,
                                                   Irufemi::Collision::CollisionResult& outResult) const {
    if (!obb) {
        outResult.isHit = false;
        return false;
    }
    // obb vs Sphere (outResult normal は Sphere を押し出す方向)
    outResult = Irufemi::Collision::GetCollisionResult(obb->GetWorldOBB(), GetWorldSphere());
    outResult.normal = Irufemi::Math::Multiply(-1.0f, outResult.normal);
    return outResult.isHit;
}

