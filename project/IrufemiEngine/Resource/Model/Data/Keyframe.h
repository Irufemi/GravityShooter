#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Quaternion.h"

namespace Irufemi {

/**
 * @struct Keyframe
 * @brief アニメーション補間カーブのキーフレームデータ構造体
 * @tparam tValue 補間対象の値の型（Vector3, Quaternion等）
 */
template <typename tValue> struct Keyframe {
    float time;   ///< キーフレーム時刻（秒）
    tValue value; ///< その時刻におけるキーフレーム値
};

/** @brief Vector3 型キーフレームのエイリアス */
using KeyframeVector3 = Keyframe<Irufemi::Vector3>;
/** @brief Quaternion 型キーフレームのエイリアス */
using KeyframeQuaternion = Keyframe<Irufemi::Quaternion>;

} // namespace Irufemi

using Irufemi::Keyframe;
using Irufemi::KeyframeQuaternion;
using Irufemi::KeyframeVector3;