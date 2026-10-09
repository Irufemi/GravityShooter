#pragma once
#include "Framework/Component/Component.h"
#include "Core/Math/Vector4.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"
#include <functional>
#include <optional>

class CollisionManager;

namespace Irufemi {
struct AABB;
namespace Collision {
struct CollisionResult;
}
}

class AABBColliderComponent;
class SphereColliderComponent;
class OBBColliderComponent;

/**
 * @class ColliderComponent
 * @brief すべての当たり判定コンポーネントの基底クラス
 */
class ColliderComponent : public Component {
public:
    enum class ColliderType { AABB, Sphere, OBB };

    virtual ~ColliderComponent();

    /**
     * @brief 開始処理 (最初のUpdate直前にCollisionManagerへ登録)
     */
    virtual void Start() override;

    /**
     * @brief 破棄処理 (CollisionManagerから登録解除)
     */
    virtual void OnDestroy() override;

    /**
     * @brief コンポーネント有効化時の通知
     */
    virtual void OnEnable() override;

    /**
     * @brief コンポーネント無効化時の通知
     */
    virtual void OnDisable() override;

    /**
     * @brief 所属するシーンが設定・変更された時の通知（シーン所属実体のみ物理登録）
     */
    virtual void OnSetScene(BaseScene* scene) override;

    /**
     * @brief Initialize を実行する。
     */
    virtual void Initialize() override {}
    /**
     * @brief Update を実行する。
     */
    virtual void Update() override {}
    /**
     * @brief Draw を実行する。
     */
    virtual void Draw() override {}

    /**
     * @brief プロパティの登録を行う
     */
    virtual void OnRegisterProperties() override;

    /**
     * @brief Serialize を実行する。
     */
    virtual nlohmann::json Serialize() override;

    /**
     * @brief Deserialize を実行する。
     */
    virtual void Deserialize(const nlohmann::json& j) override;

    /**
     * @brief コンポーネントのプロパティを別のコンポーネントからコピーする
     * @param other コピー元コンポーネント
     */
    void CopyPropertiesFrom(const Component* other) override;

    /**
     * @brief デバッグ用の当たり判定枠線（ワイヤーフレーム）を描画する
     */
    virtual void DrawDebug() = 0;

    /**
     * @brief CollisionManager を設定する。
     * @param[in] manager 設定する CollisionManager の値
     */
    static void SetCollisionManager(CollisionManager* manager) {
        collisionManager_ = manager;
    }

protected:
    inline static CollisionManager* collisionManager_ = nullptr;

public:
    /**
     * @brief 自身の当たり判定の種類（AABB, Sphere, OBB）を取得する
     * @return コライダー種別 enum
     */
    virtual ColliderType GetColliderType() const = 0;

    /**
     * @brief 空間分割（DynamicBVH）登録用のワールドAABBを取得する
     * @return ワールド空間のバウンディングボックス AABB
     */
    virtual Irufemi::AABB GetBoundingBox() const = 0;

    // =========================================================================
    // Double Dispatch Pattern (二重ディスパッチによる多態的当たり判定)
    // =========================================================================

    /**
     * @brief 第1ディスパッチ：相手のコライダーに対して自身の具象型を通知する
     * @param[in] other 判定対象のコライダー
     * @param[out] outResult 衝突結果
     * @return 衝突の有無
     */
    virtual bool TestCollision(const ColliderComponent* other,
                               Irufemi::Collision::CollisionResult& outResult) const = 0;

    /**
     * @brief 第2ディスパッチ：AABBとの衝突判定
     */
    virtual bool TestCollisionWithAABB(const AABBColliderComponent* aabb,
                                       Irufemi::Collision::CollisionResult& outResult) const = 0;

    /**
     * @brief 第2ディスパッチ：Sphereとの衝突判定
     */
    virtual bool TestCollisionWithSphere(const SphereColliderComponent* sphere,
                                         Irufemi::Collision::CollisionResult& outResult) const = 0;

    /**
     * @brief 第2ディスパッチ：OBBとの衝突判定
     */
    virtual bool TestCollisionWithOBB(const OBBColliderComponent* obb,
                                      Irufemi::Collision::CollisionResult& outResult) const = 0;

    // --- レイヤー設定 ---
    /**
     * @brief 衝突判定レイヤーを取得する
     */
    uint32_t GetLayer() const {
        return layer_;
    }
    /**
     * @brief 衝突判定レイヤーを設定する
     */
    void SetLayer(uint32_t layer) {
        layer_ = layer;
    }

    /**
     * @brief 衝突判定マスクを取得する
     */
    uint32_t GetMask() const {
        return mask_;
    }
    /**
     * @brief 衝突判定マスクを設定する
     */
    void SetMask(uint32_t mask) {
        mask_ = mask;
    }

    // --- 物理設定 ---
    /**
     * @brief トリガーモード（すり抜け判定のみ）かどうかを取得する
     */
    bool IsTrigger() const {
        return isTrigger_;
    }
    /**
     * @brief トリガーモードを設定する
     */
    void SetTrigger(bool isTrigger) {
        isTrigger_ = isTrigger;
    }

    /**
     * @brief 静的オブジェクト（押し戻されない）かどうかを取得する
     */
    bool IsStatic() const {
        return isStatic_;
    }
    /**
     * @brief 静的オブジェクトフラグを設定する
     */
    void SetStatic(bool isStatic) {
        isStatic_ = isStatic;
    }

    // --- 押し戻し軸の制限 ---
    /**
     * @brief 押し戻し軸マスクを取得する
     */
    const Irufemi::Vector3& GetPushbackMask() const {
        return pushbackMask_;
    }
    /**
     * @brief 押し戻し軸マスクを設定する
     */
    void SetPushbackMask(const Irufemi::Vector3& mask) {
        pushbackMask_ = mask;
    }

    // --- BVH (空間分割) 連携 ---
    /**
     * @brief 登録されている BVH ノード ID を取得する
     */
    int32_t GetBvhNodeId() const {
        return bvhNodeId_;
    }
    /**
     * @brief BVH ノード ID を設定する
     */
    void SetBvhNodeId(int32_t nodeId) {
        bvhNodeId_ = nodeId;
    }

    // --- コールバック設定 & 配信 ---
    /**
     * @brief 接触開始時のコールバックを設定する
     */
    void SetOnCollisionEnter(std::function<void(ColliderComponent*)> callback) {
        onCollisionEnter_ = std::move(callback);
    }
    /**
     * @brief 接触継続時のコールバックを設定する
     */
    void SetOnCollisionStay(std::function<void(ColliderComponent*)> callback) {
        onCollisionStay_ = std::move(callback);
    }
    /**
     * @brief 接触終了時のコールバックを設定する
     */
    void SetOnCollisionExit(std::function<void(ColliderComponent*)> callback) {
        onCollisionExit_ = std::move(callback);
    }

    /**
     * @brief 接触開始通知を発行する
     */
    void DispatchCollisionEnter(ColliderComponent* other) {
        if (onCollisionEnter_) {
            onCollisionEnter_(other);
        }
    }
    /**
     * @brief 接触継続通知を発行する
     */
    void DispatchCollisionStay(ColliderComponent* other) {
        if (onCollisionStay_) {
            onCollisionStay_(other);
        }
    }
    /**
     * @brief 接触終了通知を発行する
     */
    void DispatchCollisionExit(ColliderComponent* other) {
        if (onCollisionExit_) {
            onCollisionExit_(other);
        }
    }

    // --- デバッグ描画設定 ---
    /**
     * @brief デバッグプリミティブ描画時のカテゴリを取得する
     */
    DebugCategory GetDebugCategory() const {
        return debugCategory_;
    }

    /**
     * @brief デバッグプリミティブ描画時のカテゴリを設定する
     */
    void SetDebugCategory(DebugCategory category) {
        debugCategory_ = category;
    }

    /**
     * @brief デバッグ描画時のカスタムカラーを取得する (設定されていない場合は std::nullopt)
     */
    const std::optional<Irufemi::Vector4>& GetDebugCustomColor() const {
        return debugCustomColor_;
    }

    /**
     * @brief デバッグ描画時のカスタムカラーを設定する
     */
    void SetDebugCustomColor(const std::optional<Irufemi::Vector4>& color) {
        debugCustomColor_ = color;
    }

protected:
    // コールバック関数
    std::function<void(ColliderComponent*)> onCollisionEnter_;
    std::function<void(ColliderComponent*)> onCollisionStay_;
    std::function<void(ColliderComponent*)> onCollisionExit_;

    // レイヤー設定
    uint32_t layer_ = 1;         // 1 << 0 (Default)
    uint32_t mask_ = 0xFFFFFFFF; // All

    // 物理設定
    bool isTrigger_ = false; ///< trueならすり抜ける(判定のみ), falseなら物理的に押し戻す
    bool isStatic_ = false;  ///< trueなら物理的に押し戻されない（環境オブジェクトなど）

    // 押し戻し軸の制限
    Irufemi::Vector3 pushbackMask_ = {1.0f, 1.0f, 1.0f}; ///< 1.0 なら押し戻し有効, 0.0 なら無効

    // BVH (空間分割) 連携
    int32_t bvhNodeId_ = -1; //!< 自身が登録されている Irufemi::DynamicBVH 内のノードインデックス

    DebugCategory debugCategory_ = DebugCategory::Collision; //!< デバッグ描画カテゴリ (デフォルト: Collision)
    std::optional<Irufemi::Vector4> debugCustomColor_ = std::nullopt; //!< デバッグ描画カスタムカラー
};
