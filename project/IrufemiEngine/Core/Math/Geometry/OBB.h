#pragma once

#include "Core/Math/Vector3.h"
#include "Core/Math/Geometry/AABB.h"
#include <cmath>
#include <array>

namespace Irufemi {
/**
 * @class OBB
 * @brief Oriented Bounding Box (有向境界箱) を表す構造体
 * @details 任意の回転を持つ直方体の境界を定義し、より精密な衝突判定（ナローフェーズ）に使用されます。
 */
struct OBB {
    /** @brief ボックスの中心座標 */
    Vector3 center{0.0f, 0.0f, 0.0f};

    /** @brief ローカルの各軸方向を表す正規化された3つの基底ベクトル（直交必須） */
    std::array<Vector3, 3> orientations{Vector3{1.0f, 0.0f, 0.0f}, Vector3{0.0f, 1.0f, 0.0f},
                                        Vector3{0.0f, 0.0f, 1.0f}};

    /** @brief 中心から各面までの距離（各軸の長さの半分 / Extents） */
    Vector3 size{1.0f, 1.0f, 1.0f};

    /**
     * @brief OBBを包含する最小のAABBを計算する
     * @return OBBを内包するワールドAABB
     */
    [[nodiscard]] AABB ToAABB() const {
        AABB aabb;
        Vector3 extents;
        extents.x = std::abs(orientations[0].x * size.x) + std::abs(orientations[1].x * size.y) +
                    std::abs(orientations[2].x * size.z);
        extents.y = std::abs(orientations[0].y * size.x) + std::abs(orientations[1].y * size.y) +
                    std::abs(orientations[2].y * size.z);
        extents.z = std::abs(orientations[0].z * size.x) + std::abs(orientations[1].z * size.y) +
                    std::abs(orientations[2].z * size.z);

        aabb.min = {center.x - extents.x, center.y - extents.y, center.z - extents.z};
        aabb.max = {center.x + extents.x, center.y + extents.y, center.z + extents.z};
        return aabb;
    }
};
} // namespace Irufemi
