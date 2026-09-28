#pragma once

#include "Core/Math/Vector3.h"
#include <cstdint>

/**
 * @struct FogParams
 * @brief 大気・距離フォグ定数バッファ構造体 (HLSL register b0)
 */
struct FogParams {
    Irufemi::Vector3 fogColor = {0.65f, 0.70f, 0.85f}; ///< フォグ色 (淡いラベンダーブルー)
    float fogStart = 300.0f;                           ///< フォグ開始距離 (m)
    float fogEnd = 2200.0f;                            ///< フォグ終了距離 (m)
    float fogDensity = 1.0f;                           ///< フォグ密度
    uint32_t fogType = 0;                              ///< 0: Linear, 1: Exponential
    uint32_t enabled = 1;                              ///< 1: 有効, 0: 無効
};

static_assert(sizeof(FogParams) == 32, "FogParams must be 32 bytes for 16-byte alignment");
