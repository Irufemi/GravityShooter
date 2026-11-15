#pragma once

#include "Core/Math/Vector4.h"
#include "Core/Math/Matrix4x4.h"
#include <cstdint>

namespace Irufemi {

/**
 * @struct Material
 * @brief 統一マテリアル構造体
 * HLSL側の Material とメモリレイアウトを完全に一致させる
 */
struct Material {
    Vector4 color = {1.0f, 1.0f, 1.0f, 1.0f}; //!< ベースカラー
    int32_t enableLighting = 0;                        //!< ライティング有効フラグ
    int32_t hasTexture = 0;                            //!< テクスチャ有効フラグ
    int32_t lightingMode = 1;                          //!< 0:None, 1:Lambert, 2:Half-Lambert, 3:PBR
    float environmentCoefficient = 0.0f;               //!< 環境マップの映り込み係数

    Matrix4x4 uvTransform = {{{1.0f, 0.0f, 0.0f, 0.0f},
                              {0.0f, 1.0f, 0.0f, 0.0f},
                              {0.0f, 0.0f, 1.0f, 0.0f},
                              {0.0f, 0.0f, 0.0f, 1.0f}}}; //!< UV座標変換行列

    float metallic = 0.0f;       //!< 金属度
    float roughness = 0.5f;      //!< 粗さ
    int32_t useClampSampler = 0; //!< パーティクル等で使用するサンプラー切替 (0:WRAP, 1:CLAMP)
    float alphaReference = 0.0f; //!< ディスカード閾値 (0.0f = 全部描画, 1.0f = 全部棄却)

    uint32_t textureIndex = 0;      //!< [Bindless] メインテクスチャのSRVインデックス (space1)
    uint32_t envMapIndex = 0;       //!< [Bindless] 環境マップのSRVインデックス (space2)
    int32_t customEffectType = 0;   //!< カスタムエフェクトのタイプ (0: なし)
    float customEffectParam = 0.0f; //!< カスタムエフェクトのパラメータ

    int32_t enableEffectMask = 0;   //!< 1: エフェクト等のマスクバッファにシルエットを出力する
    int32_t padding[3] = {0, 0, 0}; //!< 16バイトアライメント用
};

static_assert(sizeof(Material) == 144, "Material size mismatch with HLSL");

} // namespace Irufemi
