#pragma once

#include "Core/Math/Matrix4x4.h"

namespace Irufemi {

/**
 * @struct WellForGPU
 * @brief スキニング用行列パレット（128バイト）
 * @note 現在は非一様スケールに対応するため位置用と法線用の2つの行列を保持。
 *       将来的に一様スケールモデル専用パスを導入する場合は64バイトへの削減が可能。
 */
struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;                 ///< 頂点位置変換用行列 (64B)
    Matrix4x4 skeletonSpaceInverseTransposeMatrix; ///< 法線ベクトル変換用逆転置行列 (64B)
};

} // namespace Irufemi

using Irufemi::WellForGPU;