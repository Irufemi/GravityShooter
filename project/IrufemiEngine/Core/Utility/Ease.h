#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "Core/Math/Vector4.h"
#include "Core/Math/Quaternion.h"
#include <algorithm>

namespace Irufemi {

// イージングの種類
enum class EaseType {
    Linear,
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,
    EaseInQuint,
    EaseOutQuint,
    EaseInOutQuint
};

// 指定したイージングタイプで進行度 t を評価する
/**
 * @brief EvaluateEase を実行する。
 */
float EvaluateEase(EaseType type, float t);

// 線形補間 (ジェネリックテンプレート: float, Vector2, Vector3, Vector4 等に対応)
/**
 * @brief 2つの値またはベクトル間を線形補間する
 * @param a 開始値
 * @param b 終了値
 * @param t 進行度 (0.0f ~ 1.0f)
 * @return 補間された値
 */
template <typename T> [[nodiscard]] constexpr inline T Lerp(const T& a, const T& b, float t) noexcept {
    return a + (b - a) * t;
}

// 線形補間(0~1制限あり)
/**
 * @brief 2つの値またはベクトル間を進行度0~1にクランプして線形補間する
 * @param a 開始値
 * @param b 終了値
 * @param t 進行度
 * @return クランプ補間された値
 */
template <typename T> [[nodiscard]] constexpr inline T LerpClamped(const T& a, const T& b, float t) noexcept {
    return Lerp(a, b, (std::clamp)(t, 0.0f, 1.0f));
}

// Irufemi::Quaternion 線形補間(最短経路・正規化)
/**
 * @brief クォータニオンの線形補間 (最短経路・正規化)
 */
Irufemi::Quaternion Lerp(const Irufemi::Quaternion& q1, const Irufemi::Quaternion& q2, float t);

// 球面線形補間
/**
 * @brief Slerp を実行する。
 */
Irufemi::Vector3 Slerp(const Irufemi::Vector3& v1, const Irufemi::Vector3& v2, float t);

/**
 * @brief EaseInSine を実行する。
 */
float EaseInSine(float num);

/**
 * @brief EaseOutSine を実行する。
 */
float EaseOutSine(float num);

/**
 * @brief EaseInOutSine を実行する。
 */
float EaseInOutSine(float num);

/**
 * @brief EaseInQuad を実行する。
 */
float EaseInQuad(float num);

/**
 * @brief EaseOutQuad を実行する。
 */
float EaseOutQuad(float num);

/**
 * @brief EaseInOutQuad を実行する。
 */
float EaseInOutQuad(float num);

/**
 * @brief EaseInCubic を実行する。
 */
float EaseInCubic(float num);

/**
 * @brief EaseOutCubic を実行する。
 */
float EaseOutCubic(float num);

/**
 * @brief EaseInOutCubic を実行する。
 */
float EaseInOutCubic(float num);

/**
 * @brief EaseInQuart を実行する。
 */
float EaseInQuart(float num);

/**
 * @brief EaseOutQuart を実行する。
 */
float EaseOutQuart(float num);

/**
 * @brief EaseInOutQuart を実行する。
 */
float EaseInOutQuart(float num);

/**
 * @brief EaseInQuint を実行する。
 */
float EaseInQuint(float num);

/**
 * @brief EaseOutQuint を実行する。
 */
float EaseOutQuint(float num);

} // namespace Irufemi
