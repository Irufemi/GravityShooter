/**
 * @file LightningUtility.hlsli
 * @brief 3D空間ノイズベースの表面電流・放電エフェクト計算ユーティリティ
 */

#pragma once
#ifndef LIGHTNING_UTILITY_HLSLI
#define LIGHTNING_UTILITY_HLSLI

#include "Noise.hlsli"

/**
 * @struct LightningConfig
 * @brief 表面電流のビジュアル特性を動的制御する完全パラメータ群（16バイト境界整合）
 */
struct LightningConfig {
    float density;          //!< 密度（ノイズ周波数: 小さいと大柄、大きいと細かな網目）
    float boltCount;        //!< 本数（多重レイヤー数: 1.0〜5.0）
    float thickness;        //!< 芯のシャープネス（大きいほど引き締まり細く鋭くなる）
    float speed;            //!< 疾走速度（アニメーション進行倍率）

    float intensity;        //!< 全体発光輝度（0.0で非表示、1.0で通常、20.0+で超オーバーブライト）
    float branchAmount;     //!< 支流スパークの寄与率（0.0〜1.0）
    float dissolveProgress; //!< 装甲ディゾルブ進行度（0.0で通常、1.0で完全消失）
    float dissolveEdgeWidth;//!< ディゾルブ発光エッジ幅（例: 0.08）

    float4 coreColor;       //!< 稲妻の芯の色（HDR純白・超高輝度）
    float4 haloColor;       //!< 外郭プラズマオーラ色（シアン・紫電・ネオンブルー等）
};

struct LightningSurfaceOutput {
    float3 emissiveColor;   //!< 元のマテリアル色に加算（Add）するRGB発光成分
    float boltIntensity;    //!< 稲妻の存在強度（0.0〜1.0）
};

struct LightningDissolveOutput {
    bool isDiscarded;       //!< ピクセル破棄フラグ
    float3 edgeEmission;    //!< ディゾルブ境界の高輝度プラズマエッジ発光
};

/**
 * @brief 3D空間ノイズを用いた装甲の放電ディゾルブ消滅評価関数
 * @param samplePos 3Dサンプリング座標（ワールド座標またはローカル座標）
 * @param screenPos 画面ピクセル座標（ディザリング用）
 * @param time 経過時間
 * @param cfg 設定構造体
 */
inline LightningDissolveOutput EvaluateLightningDissolve(
    float3 samplePos,
    float2 screenPos,
    float time,
    LightningConfig cfg
) {
    LightningDissolveOutput outVal;
    outVal.isDiscarded = false;
    outVal.edgeEmission = float3(0.0, 0.0, 0.0);

    if (cfg.dissolveProgress <= 0.0001) {
        return outVal;
    }

    if (cfg.dissolveProgress >= 0.9999) {
        outVal.isDiscarded = true;
        return outVal;
    }

    // 3D空間ノイズによる自然な浸食パターン (0.0〜1.0)
    float3 p = samplePos * (cfg.density * 0.35) + float3(0.0, time * 0.4, 0.0);
    float noiseVal = fBm(p);

    // インターリーブド勾配ノイズ調の微小ディザリングでアンチエイリアス感・粒子感を付加
    float dither = frac(sin(dot(screenPos, float2(12.9898, 78.233))) * 43758.5453) * 0.04;
    float combinedNoise = saturate(noiseVal + dither);

    float threshold = cfg.dissolveProgress;
    float edgeWidth = max(0.01, cfg.dissolveEdgeWidth);

    if (combinedNoise < threshold) {
        outVal.isDiscarded = true;
        return outVal;
    }

    // エッジ領域（threshold 〜 threshold + edgeWidth）で超高輝度プラズマ発光
    float edgeDist = combinedNoise - threshold;
    if (edgeDist < edgeWidth) {
        float edgeNorm = 1.0 - (edgeDist / edgeWidth);
        // エッジ光彩（haloColor と coreColor のブレンド・HDR発光）
        outVal.edgeEmission = lerp(cfg.haloColor.rgb, cfg.coreColor.rgb, edgeNorm) * (edgeNorm * edgeNorm * 18.0);
    }

    return outVal;
}

/**
 * @brief 3Dローカル座標と法線から表面電流の加算カラーを算出する純粋関数
 * @param localPos メッシュの3Dローカル座標（UV非依存）
 * @param normal 補間された法線ベクトル
 * @param viewDir 視線ベクトル（正規化済み）
 * @param time 経過時間
 * @param cfg 動的制御パラメータ
 * @return 表面電流のRGB発光成分および強度
 */
LightningSurfaceOutput Evaluate3DLightningCrawl(
    float3 localPos,
    float3 normal,
    float3 viewDir,
    float time,
    LightningConfig cfg
) {
    LightningSurfaceOutput outVal;
    
    // 輝度が実質ゼロなら早期リターン（不要なノイズ計算をスキップ）
    if (cfg.intensity <= 0.0001) {
        outVal.emissiveColor = float3(0.0, 0.0, 0.0);
        outVal.boltIntensity = 0.0;
        return outVal;
    }

    float animTime = time * cfg.speed;
    float totalBolt = 0.0;

    // --- 【レイヤー0: 主幹ボルト（低周波・太い稲妻）】 ---
    float3 p0 = localPos * cfg.density + float3(animTime * 0.45, animTime * 0.25, animTime * 0.55);
    // 3Dノイズの等値面（ゼロ交差）を電撃の筋として抽出
    float n0 = fBm(p0);
    // 0.5付近のしきい値からシャープなラインを切り出す
    float bolt0 = 1.0 - saturate(abs(n0 - 0.5) / 0.12);
    bolt0 = pow(bolt0, max(cfg.thickness, 1.0));
    totalBolt += bolt0;

    // --- 【レイヤー1: 支流スパーク（高周波・本数増加用サブオクターブ）】 ---
    if (cfg.boltCount > 1.0) {
        float branchScale = cfg.density * 2.4;
        float3 p1 = localPos * branchScale - float3(animTime * 0.7, animTime * 0.35, animTime * 0.8);
        float n1 = fBm(p1);
        float bolt1 = 1.0 - saturate(abs(n1 - 0.48) / 0.09);
        bolt1 = pow(bolt1, max(cfg.thickness * 1.4, 1.0)) * cfg.branchAmount;
        totalBolt += bolt1 * saturate(cfg.boltCount - 1.0);
    }

    totalBolt = saturate(totalBolt);

    // --- 【Fresnel外郭強調（視線に対して傾いた面で光彩をブースト）】 ---
    float fresnel = 1.0 - saturate(dot(normal, viewDir));
    float rimBoost = 1.0 + pow(fresnel, 2.5) * 1.5;

    // --- 【HDRカラー合成（芯の超高輝度純白 ＋ 外郭プラズマオーラ）】 ---
    float3 core = cfg.coreColor.rgb * pow(totalBolt, 3.2) * 8.0;
    float3 halo = cfg.haloColor.rgb * pow(totalBolt, 0.8) * 2.2;

    outVal.boltIntensity = totalBolt * cfg.intensity;
    outVal.emissiveColor = (core + halo) * rimBoost * cfg.intensity;
    return outVal;
}

#endif // LIGHTNING_UTILITY_HLSLI
