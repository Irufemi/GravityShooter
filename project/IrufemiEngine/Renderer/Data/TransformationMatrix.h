#pragma once

#include "Core/Math/Matrix4x4.h"

namespace Irufemi {

/**
 * @struct TransformationMatrix
 * @brief 3Dオブジェクトの変換行列をGPUへ送るための構造体
 */
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 world;
    Matrix4x4 WorldInverseTranspose;
};

} // namespace Irufemi
