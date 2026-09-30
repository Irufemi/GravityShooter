#pragma once
#include <vector>
#include <unordered_set>
#include <memory>
#include <string>
#include <shared_mutex>
#include <future>
#include <utility>
#include "Renderer/Object/Line/LineClass.h"
#include "Core/Math/Vector3.h"
#include "Core/Shape/LinePrimitive.h"
#include "Physics/Collision/DynamicBVH.h"
#include "Renderer/Object/Batch/DebugPrimitiveRenderer.h"

class ColliderComponent;
class GameObject;

namespace Irufemi::Collision {
struct CollisionResult;
}

struct RaycastHit {
    bool isHit = false;
    GameObject* hitObject = nullptr;
    ColliderComponent* hitCollider = nullptr;
    Irufemi::Vector3 hitPoint;
    float distance = 0.0f;
};

class ThreadPool;

class DebugPrimitiveRenderer;

/**
 * @class CollisionManager
 * @brief シーン内のすべての当たり判定を管理し、衝突判定を処理するマネージャ
 */
class CollisionManager {
public:
    CollisionManager() = default;
    ~CollisionManager();

    /// @brief 初期化
    /// @param[in] debugRenderer デバッグ描画用のレンダラーポインタ
    void Initialize(DebugPrimitiveRenderer* debugRenderer);

    /// @brief 登録されたコライダーをすべてクリアする（シーン切り替え時などに呼ぶ）
    void Clear();

    /// @brief コライダーを登録する
    void RegisterCollider(ColliderComponent* collider);

    /// @brief コライダーの登録を解除する
    void UnregisterCollider(ColliderComponent* collider);

    /// @brief 溜まった追加・削除キューを処理する
    void FlushPendingCommands();

    /// @brief 毎フレーム呼ばれ、登録された全ペアの判定を行う
    void CheckAllCollisions();

    /// @brief 全コライダーのデバッグ線を描画する
    void DrawDebug(GameObject* selectedObject = nullptr);

    /// @brief デバッグ描画フラグのポインタを取得する（ImGui用）
    bool* GetIsDrawDebugLinePtr() {
        return &isDrawDebugLine_;
    }

    // --- 動的レイヤー管理 ---
    /**
     * @brief LoadLayers を実行する。
     */
    void LoadLayers(const std::string& filepath);
    /**
     * @brief SaveLayers を実行する。
     */
    void SaveLayers(const std::string& filepath);
    /**
     * @brief LayerNames を取得する。
     * @return 取得された LayerNames
     */
    const std::vector<std::string>& GetLayerNames() const {
        return layerNames_;
    }
    /**
     * @brief AddLayer を実行する。
     */
    void AddLayer(const std::string& name);
    /**
     * @brief RemoveLayer を実行する。
     */
    void RemoveLayer(int index);
    /**
     * @brief RenameLayer を実行する。
     */
    void RenameLayer(int index, const std::string& name);

    /// @brief レイヤー名からビットマスクを取得する
    uint32_t GetLayerMask(const std::string& name) const;

    // --- レイキャスト ---
    /// @brief シーン内の全コライダーに対してレイを飛ばし、最も近いオブジェクトを返す
    /// @param ray 飛ばすレイ
    /// @param hitInfo 結果が格納される構造体
    /// @param maxDistance 判定する最大距離
    /// @param layerMask 判定対象とするレイヤーのビットマスク
    /// @param ignoreObject 判定から除外するオブジェクト（自分自身を無視するためなど）
    /// @return 何かに当たった場合はtrue
    bool Raycast(const Irufemi::Ray& ray, RaycastHit& hitInfo, float maxDistance = 1000.0f,
                 uint32_t layerMask = 0xFFFFFFFF, GameObject* ignoreObject = nullptr);

    /// @brief BVHに対してAABBのクエリを行い、交差するコライダーを取得する
    void QueryAABB(const Irufemi::AABB& aabb, std::vector<ColliderComponent*>& outHits) const;

    /**
     * @brief スレッドプールを利用した非同期レイキャスト
     * @param pool 使用するエンジンのThreadPool
     * @return 判定結果とHitInfoのペアを返すstd::future
     */
    std::future<std::pair<bool, RaycastHit>> RaycastAsync(ThreadPool* pool, const Irufemi::Ray& ray,
                                                          float maxDistance = 1000.0f, uint32_t layerMask = 0xFFFFFFFF,
                                                          GameObject* ignoreObject = nullptr);

    /// @brief デバッグ用のレイを描画キューに追加する
    void DrawDebugRay(const Irufemi::Ray& ray, float distance, const Irufemi::Vector4& color = {1, 0, 0, 1});

    /// @brief デバッグ用のAABBを描画キューに追加する
    void DrawDebugAABB(const Irufemi::AABB& aabb, const Irufemi::Vector4& color = {1, 0, 0, 1},
                       DebugCategory category = DebugCategory::Collision);

private:
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    /** @brief マルチスレッドからの物理クエリを安全に行うためのRead-Writeロック */
    mutable std::shared_mutex collidersMutex_;
    std::vector<ColliderComponent*> colliders_;

    // --- 遅延登録用キュー ---
    std::mutex pendingMutex_;                     ///< 遅延コマンド操作用ミューテックス
    std::vector<ColliderComponent*> pendingAdds_; ///< 追加予定のコライダーリスト

    struct PendingRemove {
        ColliderComponent* collider;
        int bvhNodeId;
    };
    std::vector<PendingRemove> pendingRemoves_; ///< 削除予定のコライダーリスト

    std::unique_ptr<Line3DBatch> debugLine_ = nullptr;
    DebugPrimitiveRenderer* debugPrimitiveRenderer_ = nullptr;

    // --- デバッグ用キャッシュ ---
    std::vector<ColliderComponent*> debugRemoveList_; ///< DrawDebug時の削除リスト（ヒープ割当回避用）

    // レイヤー名（最大32個）
    std::vector<std::string> layerNames_;
    std::string layerConfigFilePath_ = "resources/Config/layers.json";

    bool isDrawDebugLine_ = true;

    // Raycast描画キャッシュ
    struct DebugRayInfo {
        Irufemi::Ray ray;
        float distance;
        Irufemi::Vector4 color;
    };
    std::vector<DebugRayInfo> debugRays_;

    // 前フレームの衝突ペアを保持（Enter / Stay / Exit 用）
    struct ColliderPairHash {
        size_t operator()(const std::pair<ColliderComponent*, ColliderComponent*>& pair) const noexcept {
            uint64_t a = reinterpret_cast<uintptr_t>(pair.first);
            uint64_t b = reinterpret_cast<uintptr_t>(pair.second);
            // 64-bit 整数ミックスハッシュ (SplitMix64)
            uint64_t h = a ^ (b + 0x517cc1b727220a95ULL + (a << 6) + (a >> 2));
            return static_cast<size_t>(h);
        }
    };
    using CollisionPairSet = std::unordered_set<std::pair<ColliderComponent*, ColliderComponent*>, ColliderPairHash>;

    CollisionPairSet previousCollisions_;
    CollisionPairSet currentCollisions_;

    Irufemi::DynamicBVH dynamicBVH_;

    /**
     * @brief 2つのコライダー間の Narrow Phase（形状別詳細交差判定）を実行する
     * @param colA コライダーA
     * @param colB コライダーB
     * @param outResult 判定結果を出力する構造体
     */
    void CheckNarrowPhase(ColliderComponent* colA, ColliderComponent* colB,
                          Irufemi::Collision::CollisionResult& outResult) const;

    /**
     * @brief 衝突イベント（Enter / Stay）をコールバックおよびGameObjectへ通知する
     * @param colA コライダーA
     * @param colB コライダーB
     * @param result 衝突結果
     * @param isNewHit 新規衝突(Enter)か継続衝突(Stay)か
     */
    void DispatchCollisionEvents(ColliderComponent* colA, ColliderComponent* colB,
                                 const Irufemi::Collision::CollisionResult& result, bool isNewHit);

    /**
     * @brief 非トリガーコライダー同士のキネマティック押し戻し処理を実行する
     * @param colA コライダーA
     * @param colB コライダーB
     * @param result 衝突結果
     */
    void ResolveKinematicCollision(ColliderComponent* colA, ColliderComponent* colB,
                                   const Irufemi::Collision::CollisionResult& result);
};
