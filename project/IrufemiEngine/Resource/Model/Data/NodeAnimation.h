#pragma once

#include <vector>
#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"
#include "Resource/Model/Data/Keyframe.h"

namespace Irufemi {

/**
 * @struct AnimationCurve
 * @brief 特定の値型に関する時系列キーフレームの配列を保持するカーブ構造体
 * @tparam tValue 補間対象の値の型
 */
template <typename tValue> struct AnimationCurve {
    std::vector<Keyframe<tValue>> keyframes; ///< キーフレームリスト
};

/**
 * @struct NodeAnimation
 * @brief ボーン（ノード）1本分のTRS（平行移動・回転・拡縮）アニメーションカーブ群
 */
struct NodeAnimation {
    AnimationCurve<Irufemi::Vector3> translate; ///< 平行移動アニメーションカーブ
    AnimationCurve<Irufemi::Quaternion> rotate; ///< 回転アニメーションカーブ
    AnimationCurve<Irufemi::Vector3> scale;     ///< 拡縮アニメーションカーブ
};

} // namespace Irufemi

using Irufemi::AnimationCurve;
using Irufemi::NodeAnimation;