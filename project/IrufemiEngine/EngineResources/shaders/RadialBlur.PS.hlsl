#include "PostProcessParameters.hlsli"
#include "Fullscreen.hlsli"
#include "Bindless.hlsli"
#include "PostProcessBindlessParams.hlsli"
#include "GravityWarp.hlsli"

// ラジアルブラー用定数バッファ


ConstantBuffer<RadialBlurParams> gParams : register(b0);
SamplerState gSampler : register(s0);

struct PSOutput {
    float32_t4 color : SV_TARGET0;
};

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    
    float32_t2 uv = input.texcoord;
    
    // 重力渦（Twist）によるサンプリング起点の螺旋変位
    // blurWidth が大きいほど（加速中や吸い込み中ほど）空間がねじれる
    float twistAmount = gParams.blurWidth * 6.5f;
    float2 twistedUV = CalculateGravitySwirlUV(uv, gParams.center, twistAmount, 0.85f);
    
    // 中心から現在のUVへの方向（正規化せずに距離を含めたまま使うのがコツ）
    float32_t2 direction = twistedUV - gParams.center;
    float32_t dist = length(direction);
    
    // 中心点からの距離に応じてサンプリング数を最適化 (距離0なら1回、距離0.5以上なら最大回数)
    int32_t actualSamples = max(1, int32_t(float32_t(gParams.numSamples) * saturate(dist * 2.0f)));
    
    float32_t3 sum = 0.0f;
    
    // 放射状にサンプリングして平均化
    for (int32_t i = 0; i < actualSamples; ++i) {
        // 現在の点から中心とは逆方向にサンプリング点を進めていく
        float32_t2 sampleCoord = twistedUV + direction * gParams.blurWidth * float32_t(i);
        sum += gTexture.Sample(gSampler, sampleCoord).rgb;
    }
    
    // 平均化して出力
    output.color.rgb = sum * rcp(float32_t(actualSamples));
    output.color.a = 1.0f;
    
    return output;
}
