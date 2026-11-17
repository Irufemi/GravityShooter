#pragma once
#include "Core/Math/Vector4.h"

namespace Irufemi {

/**
 * @struct LightningConfig
 * @brief 3D空間ノイズ電流エフェクト（LightningUtility.hlsli）用の調整パラメータ構造体
 * @details HLSL側 register(b6) / RootSlot::Special の ConstantBuffer レイアウトと厳密に一致します。
 */
struct LightningConfig {
    float density = 1.5f;          //!< 密度（ノイズ周波数: 小さいと大柄、大きいと細かな網目）
    float boltCount = 1.0f;        //!< 本数（多重レイヤー数: 1.0〜5.0）
    float thickness = 3.0f;        //!< 芯のシャープネス（大きいほど引き締まる）
    float speed = 2.0f;            //!< 疾走速度（アニメーション進行倍率）

    float intensity = 0.0f;        //!< 全体発光輝度（0.0で非表示、1.0で通常、20.0+で超発光）
    float branchAmount = 0.6f;     //!< 支流スパークの寄与率（0.0〜1.0）
    float dissolveProgress = 0.0f; //!< 装甲ディゾルブ進行度（0.0で通常、1.0で完全消失）
    float dissolveEdgeWidth = 0.08f;//!< ディゾルブ発光エッジ幅

    Vector4 coreColor = {1.0f, 1.0f, 1.2f, 1.0f}; //!< 稲妻の芯の色（HDR純白・超高輝度）
    Vector4 haloColor = {0.2f, 0.6f, 1.8f, 1.0f}; //!< 外郭プラズマオーラ色（シアン・紫電等）
};

static_assert(sizeof(LightningConfig) == 64, "LightningConfig size must be 64 bytes for HLSL 16-byte alignment");

} // namespace Irufemi
