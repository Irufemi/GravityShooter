/**
 * @file GravityWarp.hlsli
 * @brief 重力レンズ・空間歪曲・重力渦（Vortex Swirl）プロシージャル計算ライブラリ
 * @details タイトル星雲（CosmicNebula）、画面遷移（RadialBlur / Transition）、
 *          およびインゲーム重力アクションで共通利用される空間幾何学歪曲関数群。
 */

#pragma once
#ifndef GRAVITY_WARP_HLSLI
#define GRAVITY_WARP_HLSLI

#include "Noise.hlsli"

/**
 * @brief 重力渦（極座標 Twist）による空間座標の回転変位を計算する
 * @param p アスペクト比補正後の中心相対座標
 * @param strength 渦の回転強度（係数）
 * @param time 時間経過による動的流動オフセット
 * @param ellipticalFactor 楕円歪み係数（Y軸のスケール、デフォルト 1.0）
 * @return 渦巻き回転後の相対座標
 */
float2 CalculateGravitySwirlCoords(float2 p, float strength, float time = 0.0f, float ellipticalFactor = 1.0f) {
    float dist = length(float2(p.x, p.y * ellipticalFactor));
    // 内周ほど急峻に吸い込まれる回転速度
    float swirlSpeed = 0.38f + 0.18f / (dist + 0.20f);
    float swirlAmount = strength * 3.6f * log(dist + 0.15f) - time * swirlSpeed;
    
    float s, c;
    sincos(swirlAmount, s, c);
    
    return float2(
        p.x * c - p.y * s,
        p.x * s + p.y * c
    );
}

/**
 * @brief 画面遷移・ポストプロセス用のUV重力歪曲オフセットを計算する
 * @param uv 元の正規化テクスチャ座標 (0.0 - 1.0)
 * @param center 歪みの中心UV座標 (例: 0.5, 0.42)
 * @param strength 歪曲強度 [0.0 - 1.0]
 * @param radius 歪曲が影響する最大半径 (UV空間)
 * @param time アニメーション時間
 * @return 空間歪曲後のサンプリングUV座標
 */
float2 CalculateGravitySwirlUV(float2 uv, float2 center, float strength, float radius = 0.85f, float time = 0.0f) {
    float2 delta = uv - center;
    float dist = length(delta);
    
    // 中心に近いほど急峻に強くなる減衰カーブ
    float factor = 1.0f - smoothstep(0.0f, radius, dist);
    float angle = strength * factor * factor * 2.5f + time * 0.5f * factor;
    
    float s, c;
    sincos(angle, s, c);
    
    float2 rotated = float2(
        delta.x * c - delta.y * s,
        delta.x * s + delta.y * c
    );
    
    return center + rotated;
}

/**
 * @brief 退廃世界・重力異常を象徴するシアン〜ディープインディゴの光彩カラーを取得する
 * @param intensity 光彩の強度 [0.0 - 1.0]
 * @return RGBAカラー (シアン〜バイオレット〜ディープブラック)
 */
float4 GetGravityAuraColor(float intensity) {
    float3 colDeep = float3(0.01f, 0.03f, 0.06f);   // 漆黒の重力深淵
    float3 colCyan = float3(0.12f, 0.85f, 0.95f);   // 鮮烈なエレクトリックシアン
    float3 colAmethyst = float3(0.35f, 0.12f, 0.55f); // 重力異常バイオレット
    
    float t = saturate(intensity);
    float3 rgb = lerp(colDeep, lerp(colCyan, colAmethyst, t * 0.4f), t);
    return float4(rgb, t);
}

#endif // GRAVITY_WARP_HLSLI
