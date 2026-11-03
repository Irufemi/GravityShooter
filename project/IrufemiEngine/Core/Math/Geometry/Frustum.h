#pragma once

#include "Core/Shape/Plane.h"
#include "Core/Math/Matrix4x4.h"
#include <array>
#include <cmath>

namespace Irufemi {
/**
 * @struct Frustum
 * @brief 視錐台を構成する6組の平面を保持する構造体
 */
struct Frustum {
    enum PlaneSide { kLeft = 0, kRight, kTop, kBottom, kNear, kFar, kNumPlanes };

    std::array<Plane, kNumPlanes> planes;

    /**
     * @brief ビュー・プロジェクション行列から視錐台の各平面を抽出する
     * @param[in] viewProjection ビュー行列と投影行列を乗算した行列
     * @details Gribb-Hartmann メソッドを使用して、抽出した平面の法線を内側（カメラ側）に向けます。
     */
    void SetFromViewProjection(const Matrix4x4& viewProjection) {
        const float (*m)[4] = viewProjection.m;

        // Left
        planes[kLeft].normal.x = m[0][3] + m[0][0];
        planes[kLeft].normal.y = m[1][3] + m[1][0];
        planes[kLeft].normal.z = m[2][3] + m[2][0];
        planes[kLeft].distance = -(m[3][3] + m[3][0]);

        // Right
        planes[kRight].normal.x = m[0][3] - m[0][0];
        planes[kRight].normal.y = m[1][3] - m[1][0];
        planes[kRight].normal.z = m[2][3] - m[2][0];
        planes[kRight].distance = -(m[3][3] - m[3][0]);

        // Top
        planes[kTop].normal.x = m[0][3] - m[0][1];
        planes[kTop].normal.y = m[1][3] - m[1][1];
        planes[kTop].normal.z = m[2][3] - m[2][1];
        planes[kTop].distance = -(m[3][3] - m[3][1]);

        // Bottom
        planes[kBottom].normal.x = m[0][3] + m[0][1];
        planes[kBottom].normal.y = m[1][3] + m[1][1];
        planes[kBottom].normal.z = m[2][3] + m[2][1];
        planes[kBottom].distance = -(m[3][3] + m[3][1]);

        // Near
        planes[kNear].normal.x = m[0][2];
        planes[kNear].normal.y = m[1][2];
        planes[kNear].normal.z = m[2][2];
        planes[kNear].distance = -(m[3][2]);

        // Far
        planes[kFar].normal.x = m[0][3] - m[0][2];
        planes[kFar].normal.y = m[1][3] - m[1][2];
        planes[kFar].normal.z = m[2][3] - m[2][2];
        planes[kFar].distance = -(m[3][3] - m[3][2]);

        // 各平面の正規化 (距離を含めて法線の長さで割る)
        for (auto& plane : planes) {
            float length = std::sqrt(plane.normal.x * plane.normal.x + plane.normal.y * plane.normal.y +
                                     plane.normal.z * plane.normal.z);
            if (length > 0.0f) {
                plane.normal.x /= length;
                plane.normal.y /= length;
                plane.normal.z /= length;
                plane.distance /= length;
            }
        }
    }
};

} // namespace Irufemi
