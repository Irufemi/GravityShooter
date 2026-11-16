#pragma once

#include <vector>
#include <cstdint>
#include <utility>
#include "Core/Math/Geometry/AABB.h"
#include "Physics/Collision/Collision.h"

namespace Irufemi {

class ColliderComponent;

/**
 * @struct BVHNode
 * @brief TLASを構成する動的AABBツリーのノード。
 *        メモリの再確保（フラグメンテーション）を防ぐため、ポインタではなくインデックスでリンクする。
 */
struct BVHNode {
    AABB aabb;
    int32_t parentIndex = -1;
    int32_t leftChildIndex = -1;
    int32_t rightChildIndex = -1;
    ColliderComponent* collider = nullptr; //!< 葉ノードの場合のみ有効

    /**
     * @brief IsLeaf かどうかを判定する。
     * @return 判定結果 (true/false)
     */
    bool IsLeaf() const {
        return leftChildIndex == -1 && rightChildIndex == -1;
    }
};

/**
 * @class DynamicBVH
 * @brief 動的AABBツリー(TLAS)を管理するクラス。
 *        配列ベースのフリーリスト(Free List)プールを用いてアロケーションをゼロにする。
 */
class DynamicBVH {
public:
    DynamicBVH();
    ~DynamicBVH() = default;

    /// @brief コライダーをツリーに挿入し、割り当てられたノードIDを返す
    int32_t Insert(ColliderComponent* collider, const AABB& aabb);

    /// @brief ノードIDを指定してツリーから削除する
    void Remove(int32_t nodeId);

    /// @brief コライダーが移動した際にAABBを更新する（位置が大きく変わった場合は再挿入される）
    void Update(int32_t nodeId, const AABB& newAABB);

    /// @brief ツリー内の交差している全ペアを抽出する（Broad-Phase）
    /// @param[out] outPairs 交差しているコライダーのペアリスト
    void GetPotentialCollisionPairs(std::vector<std::pair<ColliderComponent*, ColliderComponent*>>& outPairs) const;

    /// @brief 指定したAABBと交差するコライダーを抽出する
    void Query(const AABB& testAabb, std::vector<ColliderComponent*>& outColliders) const;

    /// @brief レイキャスト走査を行い、ヒットしたコライダーを抽出する
    void RaycastQuery(const Ray& ray, float maxDistance, std::vector<ColliderComponent*>& outHits) const;

    /// @brief ツリーを空にする
    void Clear();

private:
    std::vector<BVHNode> nodes_;
    int32_t rootIndex_ = -1;
    int32_t freeListFirst_ = -1;

    // --- 内部ヘルパー関数 ---

    /// @brief 新しいノードを確保（またはフリーリストから再利用）する
    int32_t AllocateNode();

    /// @brief ノードをフリーリストに返却する
    void FreeNode(int32_t nodeId);

    /// @brief 2つのAABBを結合した新しいAABBを返す
    AABB MergeAABB(const AABB& a, const AABB& b) const;

    /// @brief AABBの表面積（Surface Area）を計算する
    float SurfaceArea(const AABB& aabb) const;

    /// @brief 葉ノードをツリーに挿入する
    void InsertLeaf(int32_t leafIndex);

    /// @brief 葉ノードをツリーから削除する
    void RemoveLeaf(int32_t leafIndex);

    /// @brief 下から上に向かってAABBを再計算（Refit）する
    void Refit(int32_t nodeIndex);

    /// @brief 単一部分木内の自己衝突ペア抽出の再帰処理
    void ComputeSelfPairs(int32_t nodeIndex,
                          std::vector<std::pair<ColliderComponent*, ColliderComponent*>>& outPairs) const;

    /// @brief 異なる2つの部分木間のペア抽出の再帰処理
    void ComputePairs(int32_t node0, int32_t node1,
                      std::vector<std::pair<ColliderComponent*, ColliderComponent*>>& outPairs) const;

    /// @brief AABB同士の交差判定
    bool Intersects(const AABB& a, const AABB& b) const;
};

} // namespace Irufemi
