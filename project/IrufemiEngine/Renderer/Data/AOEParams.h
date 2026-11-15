#pragma once

namespace Irufemi {

/**
 * @struct AOEParams
 * @brief AOEWarning シェーダーへ渡す専用パラメータ構造体
 */
struct AOEParams {
    int shapeType = 0;         //!< 形状タイプ (0: 円形/Radial, 1: 直線/Linear)
    float warningRatio = 0.0f; //!< 警告の進行度 (0.0 ~ 1.0)
    float pad[2] = {0, 0};     //!< 16バイトアライメント用パディング
};

static_assert(sizeof(AOEParams) % 16 == 0, "AOEParams must be 16-byte aligned for HLSL constant buffer");

} // namespace Irufemi
