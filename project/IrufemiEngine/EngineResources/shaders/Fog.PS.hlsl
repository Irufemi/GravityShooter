#include "PerFrame.hlsli"
#include "Bindless.hlsli"

/**
 * @struct FogParams
 * @brief フォグ定数バッファ (register b0)
 */
struct FogParams {
    float3 fogColor;       ///< フォグ色 (大気・スカイカラー)
    float fogStart;        ///< フォグ開始距離 (m)
    float fogEnd;          ///< フォグ終了距離 (m)
    float fogDensity;      ///< 密度
    uint fogType;          ///< 0: Linear, 1: Exponential
    uint enabled;          ///< 1: 有効, 0: 無効
};

ConstantBuffer<FogParams> gFog : register(b0);
ConstantBuffer<PerFrameData> gPerFrame : register(b2);
SamplerState gSamplerPoint : register(s1);

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    if (gFog.enabled == 0) {
        discard;
    }

    // 深度バッファをサンプリング
    float ndcDepth = gTextures[gPerFrame.depthMapIndex].Sample(gSamplerPoint, input.texcoord).r;

    // 最奥深度（プロシージャル天球・空）はフォグ適用対象外のため破棄
    if (ndcDepth >= 0.999999f) {
        discard;
    }

    // パースペクティブ投影行列成分からカメラ距離（ビュー空間深度）を高速高精度に復元
    float p22 = gPerFrame.projection[2][2];
    float p32 = gPerFrame.projection[3][2];
    float viewZ = p32 / (ndcDepth - p22);
    float dist = abs(viewZ);

    // 線形フォグ率 f を算出 [0.0, 1.0]
    float f = saturate((dist - gFog.fogStart) / max(0.001f, gFog.fogEnd - gFog.fogStart));

    // ハードウェアアルファブレンド (SrcAlpha, InvSrcAlpha) によりメインバッファへ合成
    return float4(gFog.fogColor, f);
}
