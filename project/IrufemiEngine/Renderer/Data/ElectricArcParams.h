#pragma once
#include "Core/Math/Vector4.h"

namespace Irufemi {

/**
 * @struct ElectricArcParams
 * @brief 3D空間量子化アーク放電シェーダー（ElectricArc.PS.hlsl）用のパラメータ構造体
 * @details HLSL側 register(b6) / RootSlot::Special の ConstantBuffer レイアウトと厳密に一致（64バイト）
 */
struct ElectricArcParams {
    Vector4 arcColor = {1.8f, 0.2f, 2.2f, 1.0f};  //!< 稲妻の外側グロー色 (マゼンタ)
    Vector4 coreColor = {1.8f, 1.8f, 2.0f, 1.0f}; //!< 稲妻の超高輝度芯 (白/HDR)
    float intensity = 1.0f;                       //!< 全体輝度
    float noiseScale = 0.8f;                      //!< 放電の空間周波数 (密度)
    float jitterSpeed = 30.0f;                    //!< 毎秒の跳躍回数 (約25.0〜35.0)
    float boltThinness = 0.12f;                   //!< 稲妻の細さ (極薄スリット幅 0.05〜0.15)
    float branchDensity = 0.6f;                   //!< 枝分かれの密度 (サブボルト)
    float pad[3] = {0.0f, 0.0f, 0.0f};            //!< 16バイトアライメント
};

static_assert(sizeof(ElectricArcParams) == 64, "ElectricArcParams size must be 64 bytes");

} // namespace Irufemi
