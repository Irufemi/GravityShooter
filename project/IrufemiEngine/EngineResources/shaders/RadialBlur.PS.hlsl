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
    // 過度なねじれを抑え、上品で滑らかな重力レンズ引き込みを実現
    float twistAmount = gParams.blurWidth * 2.2f;
    float2 twistedUV = CalculateGravitySwirlUV(uv, gParams.center, twistAmount, 0.65f);
    
    // 中心から現在のUVへの方向（求心的吸い込み: 中心へ向かって光が収束する）
    float32_t2 direction = twistedUV - gParams.center;
    float32_t dist = length(direction);
    
    // 中心点からの距離に応じてサンプリング数を最適化
    int32_t actualSamples = max(2, int32_t(float32_t(gParams.numSamples) * saturate(dist * 2.5f)));
    
    float32_t3 sum = 0.0f;
    
    // 中心へ向かって（求心性インワード・ワープ）放射状にサンプリングして平均化
    for (int32_t i = 0; i < actualSamples; ++i) {
        float stepFraction = float32_t(i) / float32_t(actualSamples);
        // 中心方向へ滑らかに引き込むサンプリング座標
        float32_t2 sampleCoord = twistedUV - direction * (gParams.blurWidth * stepFraction * 2.5f);
        sum += gTexture.Sample(gSampler, sampleCoord).rgb;
    }
    
    // 平均化して出力
    output.color.rgb = sum * rcp(float32_t(actualSamples));
    output.color.a = 1.0f;
    
    return output;
}
