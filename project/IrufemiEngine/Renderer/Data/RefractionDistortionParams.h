#pragma once
#include "Core/Math/Vector4.h"
#include <cstdint>

namespace Irufemi {

/**
 * @struct RefractionDistortionParams
 * @brief 3D空間個別屈折シェーダー（RefractionDistortion.PS.hlsl）用のパラメータ構造体
 * @details HLSL側 register(b6) / RootSlot::Special の ConstantBuffer レイアウトと厳密に一致（64バイト）
 */
struct RefractionDistortionParams {
    Vector4 distortionTint = {1.0f, 1.0f, 1.0f, 1.0f}; //!< 屈折領域のカラー色調（重力シアンや電磁マゼンタ）
    float distortionIntensity = 0.08f;                 //!< 屈折の強さ (0.01〜0.20)
    float waveRadius = 0.0f;                           //!< 衝撃波リングの半径 [0.0〜1.0] (ローカルクアッド空間)
    float waveWidth = 0.15f;                           //!< 衝撃波リングの幅
    float chromaticAberration = 0.02f;                 //!< 局所色収差（プリズムRGBズレ）
    uint32_t sceneTextureIndex = 0;                    //!< BindlessシーンカラーSRVインデックス
    float pinchIntensity = 0.5f;                       //!< 中心への引き込み（ブラックホール的ピンチ）強度
    float time = 0.0f;                                 //!< アニメーション時間
    float pad[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};     //!< 16バイトアライメントパディング (合計64バイト)
};

static_assert(sizeof(RefractionDistortionParams) == 64, "RefractionDistortionParams size must be 64 bytes");

} // namespace Irufemi
