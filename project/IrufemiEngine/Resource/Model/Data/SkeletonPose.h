#pragma once

#include "Core/Math/QuaternionTransform.h"
#include "Core/Math/Matrix4x4.h"
#include <vector>
#include <cstdint>
#include <memory>

namespace Irufemi {

struct SkeletonData;
struct NodeAnimation;

/**
 * @struct JointPose
 * @brief 各インスタンスごとの動的なボーンの状態
 */
struct JointPose {
    QuaternionTransform transform; // 現在のローカルTransform
    Matrix4x4 localMatrix;         // 現在のローカル行列
    Matrix4x4 skeletonSpaceMatrix; // スケルトン空間（ルートからの累積）行列
};

/**
 * @struct SkeletonPose
 * @brief 各モデル（インスタンス）が持つ現在の姿勢情報
 */
struct SkeletonPose {
    const SkeletonData* data = nullptr; // 参照する静的データ
    std::vector<JointPose> jointPoses;

    // --- 最適化用キャッシュ ---
    // 最後に適用したアニメーションの固有ID（変更検知用、ABA問題対策）
    uint64_t lastAppliedAnimationId = 0;
    // Jointインデックスと対象NodeAnimationのポインタを紐付けたリスト
    std::vector<std::pair<int32_t, const NodeAnimation*>> activeAnimationBindings;

    // --- ブレンド用最適化キャッシュ ---
    uint64_t lastBlendAnimAId = 0;
    uint64_t lastBlendAnimBId = 0;
    // Jointインデックス, NodeAnimA, NodeAnimB (どちらかがnullptrの場合もある)
    std::vector<std::tuple<int32_t, const NodeAnimation*, const NodeAnimation*>> activeBlendBindings;
};

} // namespace Irufemi
