#pragma once
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Core/Math/Geometry/AABB.h"
#include "Core/Math/Vector3.h"
#include <string>

class TransformComponent;

/**
 * @class AABBColliderComponent
 * @brief 軸に平行な箱型の当たり判定コンポーネント
 */
class AABBColliderComponent : public ColliderComponent {
public:
    AABBColliderComponent();
    ~AABBColliderComponent() override = default;

    /**
     * @brief DrawDebug を実行する。
     */
    void DrawDebug() override;

    /**
     * @brief プロパティの登録
     */
    void OnRegisterProperties() override;
    /**
     * @brief Serialize を実行する。
     */
    nlohmann::json Serialize() override;
    /**
     * @brief Deserialize を実行する。
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief ComponentName を取得する。
     * @return 取得された ComponentName
     */
    std::string GetComponentName() const override {
        return "AABBColliderComponent";
    }
    /**
     * @brief ColliderType を取得する。
     * @return 取得された ColliderType
     */
    ColliderType GetColliderType() const override {
        return ColliderType::AABB;
    }
    /**
     * @brief BoundingBox を取得する。
     * @return 取得された BoundingBox
     */
    Irufemi::AABB GetBoundingBox() const override {
        return GetWorldAABB();
    }

    // --- Double Dispatch Pattern ---
    bool TestCollision(const ColliderComponent* other,
                       Irufemi::Collision::CollisionResult& outResult) const override;
    bool TestCollisionWithAABB(const AABBColliderComponent* aabb,
                               Irufemi::Collision::CollisionResult& outResult) const override;
    bool TestCollisionWithSphere(const SphereColliderComponent* sphere,
                                 Irufemi::Collision::CollisionResult& outResult) const override;
    bool TestCollisionWithOBB(const OBBColliderComponent* obb,
                              Irufemi::Collision::CollisionResult& outResult) const override;

    /// @brief ワールド空間上での現在のAABBを取得
    Irufemi::AABB GetWorldAABB() const;

    /**
     * @brief ボックスの中心からのズレ（ローカル座標）を設定します
     * @param offset 設定するオフセット値
     */
    void SetLocalOffset(const Irufemi::Vector3& offset) {
        localOffset_ = offset;
    }

    /**
     * @brief ボックスの中心からのズレ（ローカル座標）を取得します
     * @return const Irufemi::Vector3& オフセット値
     */
    const Irufemi::Vector3& GetLocalOffset() const {
        return localOffset_;
    }

    /**
     * @brief ボックスの半幅（Extents）を設定します
     * @param size 設定する半幅
     */
    void SetLocalSize(const Irufemi::Vector3& size) {
        localSize_ = size;
    }

    /**
     * @brief ボックスの半幅（Extents）を取得します
     * @return const Irufemi::Vector3& 半幅
     */
    const Irufemi::Vector3& GetLocalSize() const {
        return localSize_;
    }

private:
    Irufemi::Vector3 localOffset_ = {0.0f, 0.0f, 0.0f}; //!< 中心からのズレ
    Irufemi::Vector3 localSize_ = {1.0f, 1.0f, 1.0f};   //!< ボックスの半幅（Extents）
};
