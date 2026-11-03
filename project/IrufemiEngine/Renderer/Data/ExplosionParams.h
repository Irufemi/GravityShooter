#pragma once
#include "Core/Math/Vector4.h"
#include "Core/Math/Vector3.h"
/**
 * @struct ExplosionParams
 * @brief 爆炎エフェクト（Explosion Flame）の調整用パラメータ構造体
 * @note RootSignatureの b6 (RootSlot::Special) にバインドされる想定
 */
struct ExplosionParams {
    Irufemi::Vector4 edgeColor = {0.8f, 0.1f, 0.0f, 1.0f}; //!< 炎の先端・外側の色
    Irufemi::Vector4 midColor = {1.0f, 0.5f, 0.1f, 1.0f};  //!< 炎の中間色
    Irufemi::Vector4 coreColor = {1.0f, 1.0f, 0.9f, 1.0f}; //!< 炎の中心・芯の色
    float speed = 5.0f;                                    //!< 炎のうねり（ノイズ）アニメーション速度
    float intensity = 4.0f;                                //!< 全体の発光強度
    float noiseScale = 3.0f;                               //!< ノイズの密度
    float erosion = 0.2f;                                  //!< 浸食度合い（煙や消え際の表現用）
    Irufemi::Vector3 sphereCenter = {0, 0, 0};             //!< 爆発球の中心座標 (Raymarching用)
    float sphereRadius = 10.0f;                            //!< 爆発球の半径 (Raymarching用)
    float pad[44];                                         //!< 256バイトアライメント用のパディング
};

static_assert(sizeof(ExplosionParams) == 256, "ExplosionParams size must be 256 bytes");
