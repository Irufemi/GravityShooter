#include "PostProcessParameters.hlsli"
#include "Fullscreen.hlsli"
#include "Bindless.hlsli"
#include "PostProcessBindlessParams.hlsli"
#include "SpaceTransforms.hlsli"



ConstantBuffer<OutlineParams> gOutline : register(b0);
SamplerState gSampler : register(s0);
SamplerState gSamplerPoint : register(s1);



static const float32_t kPrewittHorizontalKernel[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};

static const float32_t kPrewittVerticalKernel[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

static const int32_t2 kIndex3x3[3][3] = {
    {{-1, -1}, {0, -1}, {1, -1}},
    {{-1,  0}, {0,  0}, {1,  0}},
    {{-1,  1}, {0,  1}, {1,  1}},
};

PixelShaderOutput main(VertexShaderOutput input) {
    uint32_t width, height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(1.0f / width, 1.0f / height);

    float32_t2 depthDiff = float32_t2(0.0f, 0.0f);
    float32_t3 normalDiffX = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t3 normalDiffY = float32_t3(0.0f, 0.0f, 0.0f);
    
    for (int32_t x = 0; x < 3; ++x) {
        for (int32_t y = 0; y < 3; ++y) {
            float32_t2 texcoord = input.texcoord + kIndex3x3[x][y] * uvStepSize;
            
            // 深度のサンプリング
            float32_t ndcDepth = gExtraTexture.Sample(gSamplerPoint, texcoord).r;
            float32_t viewZ = ReconstructViewZ(ndcDepth, gOutline.projectionInverse);
            
            // 法線のサンプリング
            float32_t3 normal = gNormalTexture.Sample(gSamplerPoint, texcoord).xyz;
            
            depthDiff.x += viewZ * kPrewittHorizontalKernel[x][y];
            depthDiff.y += viewZ * kPrewittVerticalKernel[x][y];
            
            normalDiffX += normal * kPrewittHorizontalKernel[x][y];
            normalDiffY += normal * kPrewittVerticalKernel[x][y];
        }
    }

    // 変化の長さをウェイトとして合成
    // 深度と法線のエッジをそれぞれ適度な係数で合成（バンド・デシネ風に法線を強めに拾う）
    float32_t depthWeight = length(depthDiff) * 2.0f;
    float32_t normalWeight = (length(normalDiffX) + length(normalDiffY)) * 1.5f;
    float32_t edgeVal = depthWeight + normalWeight;
    
    // アメコミ・トゥーン調のパキッとしたインク線を引くため、二値化(smoothstep)する
    // gOutline.intensity をしきい値の逆数として活用する
    float edgeThreshold = 0.5f / max(0.001f, gOutline.intensity);
    
    // 0.0(線なし) ~ 1.0(インク線)
    float inkLine = smoothstep(edgeThreshold - 0.05f, edgeThreshold + 0.05f, edgeVal);

    PixelShaderOutput output;
    // エッジ部分を深いチャコールインク色で乗算合成
    float32_t3 inkColor = float32_t3(0.02f, 0.02f, 0.03f);
    output.color.rgb = lerp(gTexture.Sample(gSampler, input.texcoord).rgb, inkColor, inkLine);
    output.color.a = 1.0f;
    
    return output;
}
