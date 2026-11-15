#pragma once
#include "Core/Math/Vector4.h"

namespace Irufemi {

/**
 * @struct LightningParams
 * @brief 電撃エフェクト（Lightning Crawl）の調整用パラメータ構造体
 */
struct LightningParams {
    Vector4 color = {0.8f, 0.4f, 1.0f, 1.0f};     //!< 表面の色
    Vector4 coreColor = {1.0f, 1.0f, 1.2f, 1.0f}; //!< 内部（芯）の色
    float speed = 1.0f;                                    //!< アニメーション速度
    float intensity = 1.0f;                                //!< 表面の輝度
    float noiseScale = 1.0f;                               //!< 表面ノイズ密度
    float noiseThreshold = 0.5f;                           //!< 表面の出現しきい値

    float coreIntensity = 2.0f; //!< 芯の輝度
    float coreThreshold = 0.7f; //!< 芯の太さ
    float coreScale = 2.0f;     //!< 芯のノイズ密度
    float spinSpeed = 0.0f;     //!< 横回転（螺旋）の速度
    float twistScale = 0.0f;    //!< 螺旋のねじれの強さ
    float pad[3] = {0, 0, 0};   //!< 16バイトアライメント用パディング
};

static_assert(sizeof(LightningParams) == 80, "LightningParams size must be 80 bytes");

} // namespace Irufemi
