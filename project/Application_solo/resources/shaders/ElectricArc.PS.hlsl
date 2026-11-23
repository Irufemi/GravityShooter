#include "Transform.hlsli"
#include "BasePassVertexOutput.hlsli"
#include "Noise.hlsli"

/**
 * @file ElectricArc.PS.hlsl
 * @brief 3D空間座標に基づきモデル表面に角ばった物理アーク放電（稲妻）を描画するピクセルシェーダー
 * @details
 * UV展開に依存せず、ワールド座標からドメインワーピングと階段状量子化ノイズを用いて
 * 鋭利に折れ曲がり毎秒30回瞬間跳躍するリアルな高圧アーク放電を生成します。
 */

// 特殊パラメータ (register b6 / RootSlot::Special)
struct ElectricArcParams {
    float32_t4 arcColor;        //!< 稲妻の外側グロー色 (シアン/マゼンタ/アンバー)
    float32_t4 coreColor;       //!< 稲妻の超高輝度芯 (白/HDR)
    float32_t intensity;        //!< 全体輝度
    float32_t noiseScale;       //!< 放電の空間周波数 (密度)
    float32_t jitterSpeed;      //!< 毎秒の跳躍回数 (約25.0〜35.0)
    float32_t boltThinness;     //!< 稲妻の細さ (極薄スリット幅 0.05〜0.15)
    float32_t branchDensity;    //!< 枝分かれの密度 (サブボルト)
    float32_t pad[3];
};
ConstantBuffer<ElectricArcParams> gArcParams : register(b6);

#include "PerFrame.hlsli"
ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct PixelShaderOutput {
    float32_t4 color : SV_TARGET0;
};

// 階段状量子化とドメインワーピングによる鋭利なアーク放電フィールド
float EvaluateArcNoise(float3 worldPos, float discreteTime, float scale) {
    // 3Dドメインワーピング（座標を別のノイズで空間的にねじ曲げる）
    float3 warp = float3(
        fBm(worldPos * 0.4 + float3(0.0, discreteTime * 0.7, 13.5)),
        fBm(worldPos * 0.4 + float3(discreteTime * 0.7, 17.1, 0.0)),
        fBm(worldPos * 0.4 + float3(23.4, 0.0, discreteTime * 0.7))
    );
    
    // カクカクとした折れ線にするため、座標を階段状に量子化（Quantization）
    float3 jaggedP = floor((worldPos + warp * 2.2) * 5.0) / 5.0;
    
    return fBm(jaggedP * scale);
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // 時間をステップ化して、1秒間に指定回数（約30回）経路を瞬間跳躍（Jitter）させる
    float discreteTime = floor(gPerFrame.time * gArcParams.jitterSpeed);

    // 1. メインボルト（主放電アーク）
    float n1 = EvaluateArcNoise(input.worldPosition, discreteTime, gArcParams.noiseScale);
    // 0.5付近の極薄スリットを抽出
    float mainBolt = 1.0 - saturate(abs(n1 - 0.5) / (gArcParams.boltThinness * 0.25));
    mainBolt = pow(mainBolt, 4.0); // 芯を極めて鋭く

    // 2. サブボルト（細い枝分かれアーク）
    float n2 = EvaluateArcNoise(input.worldPosition + float3(7.3, 11.1, 5.7), discreteTime + 100.0, gArcParams.noiseScale * 1.8);
    float subBolt = 1.0 - saturate(abs(n2 - 0.5) / (gArcParams.boltThinness * 0.40));
    subBolt = pow(subBolt, 3.0) * gArcParams.branchDensity;

    // 放電の統合
    float totalBolt = saturate(mainBolt + subBolt);

    if (totalBolt <= 0.015) {
        discard; // 放電のない部分は完全透過して下地メッシュを見せる
    }

    // 3. HDR発光の合成
    // 芯（Core）は超高輝度ホワイト
    float3 rgb = gArcParams.coreColor.rgb * pow(totalBolt, 2.0) * gArcParams.intensity * 6.0;
    // 外郭（Glow）はテーマカラー（シアン/マゼンタ/アンバー等）
    rgb += gArcParams.arcColor.rgb * totalBolt * gArcParams.intensity * 2.0;

    // アルファは加算合成用（ブルームと加算ブレンドに対応）
    float alpha = saturate(totalBolt * 1.5) * gArcParams.arcColor.a;

    output.color = float32_t4(rgb, alpha);
    return output;
}
