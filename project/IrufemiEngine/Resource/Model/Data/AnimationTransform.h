#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"

namespace Irufemi {

/**
 * @struct AnimationTransform
 * @brief アニメーション用のトランスフォームデータを保持する構造体
 */
struct AnimationTransform {
    Irufemi::Vector3 scale_;     ///< スケール
    Irufemi::Quaternion rotate_; ///< 回転クォータニオン
    Irufemi::Vector3 translate_; ///< 平行移動
};

} // namespace Irufemi

using Irufemi::AnimationTransform;
