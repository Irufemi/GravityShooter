#pragma once
#include "Framework/Component/Collider/ColliderComponent.h"
#include "Core/Shape/Sphere.h"
#include "Core/Math/Vector3.h"
#include <string>

class TransformComponent;

/**
 * @class SphereColliderComponent
 * @brief 球体を用いた衝突判定を行うコンポーネント
 * @details GameObjectにアタッチし、全方位で均等な半径を持つ当たり判定領域を提供します。
 */
class SphereColliderComponent : public ColliderComponent {
public:
    /**
     * @brief コンストラクタ
     */
    SphereColliderComponent();

    /**
     * @brief デストラクタ
     */
    ~SphereColliderComponent() override = default;

    void OnRegisterProperties() override;

    /**
     * @brief デバッグ描画
     * @details 衝突領域をワイヤーフレーム等で可視化します。
     */
    void DrawDebug() override;

    /**
     * @brief シリアライズ
     * @return 球の半径やオフセット情報を含むJSONオブジェクト
     */
    nlohmann::json Serialize() override;

    /**
     * @brief デシリアライズ
     * @param[in] j 読み込むJSONオブジェクト
     */
    void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief クローンを作成する
     */
    std::shared_ptr<Component> Clone() override;

    /**
     * @brief コンポーネント名の取得
     * @return "SphereColliderComponent"
     */
    std::string GetComponentName() const override {
        return "SphereColliderComponent";
    }

    /**
     * @brief コライダーの型を取得
     * @return ColliderType::Sphere
     */
    ColliderType GetColliderType() const override {
        return ColliderType::Sphere;
    }

    /**
     * @brief 衝突判定用のブロードフェーズバウンディングボックスを取得
     * @return ワールド空間のAABB
     */
    Irufemi::AABB GetBoundingBox() const override;

    // --- Double Dispatch Pattern ---
    bool TestCollision(const ColliderComponent* other, Irufemi::Collision::CollisionResult& outResult) const override;
    bool TestCollisionWithAABB(const AABBColliderComponent* aabb,
                               Irufemi::Collision::CollisionResult& outResult) const override;
    bool TestCollisionWithSphere(const SphereColliderComponent* sphere,
                                 Irufemi::Collision::CollisionResult& outResult) const override;
    bool TestCollisionWithOBB(const OBBColliderComponent* obb,
                              Irufemi::Collision::CollisionResult& outResult) const override;

    /**
     * @brief ワールド空間での球体情報を取得する
     * @return 計算済みのワールドSphere
     */
    Irufemi::Sphere GetWorldSphere() const;

    /**
     * @brief ローカル空間でのオフセット座標を設定する
     * @param[in] offset ローカル座標系の中心からのズレ
     */
    void SetLocalOffset(const Irufemi::Vector3& offset) {
        localOffset_ = offset;
    }

    /**
     * @brief ローカル空間でのオフセット座標を取得する
     * @return ローカル座標系の中心からのズレ
     */
    const Irufemi::Vector3& GetLocalOffset() const {
        return localOffset_;
    }

    /**
     * @brief ローカル空間での球の半径を設定する
     * @param[in] radius 半径
     */
    void SetLocalRadius(float radius) {
        localRadius_ = radius;
    }

    /**
     * @brief ローカル空間での球の半径を取得する
     * @return 半径
     */
    float GetLocalRadius() const {
        return localRadius_;
    }

private:
    Irufemi::Vector3 localOffset_ = {0.0f, 0.0f, 0.0f};
    float localRadius_ = 1.0f;
};
