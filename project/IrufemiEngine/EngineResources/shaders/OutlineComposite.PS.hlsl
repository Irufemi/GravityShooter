#include "Fullscreen.hlsli"
#include "Bindless.hlsli"
#include "PostProcessBindlessParams.hlsli"
#include "PerFrame.hlsli"

SamplerState smp : register(s0);

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

float4 main(VertexShaderOutput input) : SV_TARGET {
    float2 offset = 1.0f / gPerFrame.resolution;
    
    // 画面端のピクセルでは強制的にエッジを無視する（画面ふちに線が出るのを防ぐ）
    if (input.texcoord.x <= offset.x || input.texcoord.x >= 1.0f - offset.x ||
        input.texcoord.y <= offset.y || input.texcoord.y >= 1.0f - offset.y) {
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // 単純な十字サンプリングによる膨張（ブラー・エッジ検出）
    float center = gTexture.Sample(smp, input.texcoord).r;
    float up     = gTexture.Sample(smp, input.texcoord + float2(0, -offset.y)).r;
    float down   = gTexture.Sample(smp, input.texcoord + float2(0,  offset.y)).r;
    float left   = gTexture.Sample(smp, input.texcoord + float2(-offset.x, 0)).r;
    float right  = gTexture.Sample(smp, input.texcoord + float2( offset.x, 0)).r;
    
    // エッジを検出 (周囲が白で中心が黒、またはその逆)
    float edge = abs((up + down + left + right) - center * 4.0f);
    
    // エッジの強さに応じてオレンジ色 (1.0, 0.5, 0.0) を出力する（背景は edge=0 で透明になる）
    return float4(1.0f * edge, 0.5f * edge, 0.0f, edge);
}
