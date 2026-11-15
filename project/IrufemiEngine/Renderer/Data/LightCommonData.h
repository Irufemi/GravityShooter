#pragma once

#include "Renderer/Data/DirectionalLight.h"
#include "Core/Math/Matrix4x4.h"
#include <cstdint>

namespace Irufemi {

/**
 * @struct LightCommonData
 * @brief ライトに関連する共通情報を格納する定数バッファ用構造体
 */
struct LightCommonData {
    DirectionalLight directionalLight; //!< 平行光源 (1体固定)
    Matrix4x4 viewProjection; //!< ライト視点の投影行列 (ShadowMap用)
    uint32_t pointLightCount;          //!< 有効な点光源の数
    uint32_t spotLightCount;           //!< 有効なスポットライトの数
    uint32_t areaLightCount;           //!< 有効なエリアライトの数

    // [Bindless] ライト用 StructuredBuffer のインデックス
    uint32_t pointLightBufferIndex; //!< 点光源バッファ (StructuredBuffer, space4)
    uint32_t spotLightBufferIndex;  //!< スポットライトバッファ (StructuredBuffer, space5)
    uint32_t areaLightBufferIndex;  //!< エリアライトバッファ (StructuredBuffer, space6)
    uint32_t padding[2];            //!< パディング (16バイトアライメント用)
};

static_assert(sizeof(LightCommonData) == 128, "LightCommonData size mismatch with HLSL");

} // namespace Irufemi
