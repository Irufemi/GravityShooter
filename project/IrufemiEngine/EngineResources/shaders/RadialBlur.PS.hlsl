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

// 高周波インターリーブド・グラディエントノイズ（ピクセル単位のディザリングジッター）
// 階段状の区切り・同心円状のバンディングを人間の目に知覚できない完全なボケへと平滑化
float InterleavedGradientNoise(float2 screenPos) {
    return frac(52.9829189f * frac(dot(screenPos, float2(0.06711056f, 0.00583715f))));
}

PSOutput main(VertexShaderOutput input) {
    PSOutput output;
    
    float32_t2 uv = input.texcoord;
    
    // 重力渦（Twist）によるサンプリング起点の螺旋変位
    // 過度なねじれを抑え、上品で滑らかな重力レンズ引き込みを実現
    float twistAmount = gParams.blurWidth * 2.2f;
    float2 twistedUV = CalculateGravitySwirlUV(uv, gParams.center, twistAmount, 0.65f);
    
    // 中心から現在のUVへの方向（求心的吸い込み: 中心へ向かって光が収束する）
    float32_t2 direction = twistedUV - gParams.center;
    
    // 中心特異点付近でも粗くならないよう最低16サンプルを保証
    int32_t actualSamples = max(16, int32_t(gParams.numSamples));
    
    float dither = InterleavedGradientNoise(input.position.xy);
    float32_t3 sum = 0.0f;
    
    // 中心へ向かって（求心性インワード・ワープ）放射状にサンプリングして平均化
    for (int32_t i = 0; i < actualSamples; ++i) {
        // ジッターオフセットを重畳し、サンプリング間隔のステップ（粗い区切り）を完全消滅
        float stepFraction = (float(i) + dither) / float(actualSamples);
        float32_t2 sampleCoord = twistedUV - direction * (gParams.blurWidth * stepFraction);
        sum += gTexture.Sample(gSampler, sampleCoord).rgb;
    }
    
    // 平均化して出力
    output.color.rgb = sum * rcp(float32_t(actualSamples));
    output.color.a = 1.0f;
    
    return output;
}
