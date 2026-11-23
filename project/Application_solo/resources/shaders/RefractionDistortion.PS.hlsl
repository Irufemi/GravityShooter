/**
 * @file RefractionDistortion.PS.hlsl
 * @brief 3Dワールド空間 個別空間屈折（Screen-Space Refraction）ピクセルシェーダー
 * @details 全画面ポストプロセス（LightningCataclysm/GravityDistortion）の画面全体ジャックから脱却し、
 *          3D空間上のクアッド／ビルボードが存在する局所領域（半径数メートル）の中だけ、
 *          背後のシーンカラーをブラックホール的ピンチ引き込み ＆ 衝撃波リングで屈折させます。
 */

#include "BasePassVertexOutput.hlsli"
#include "PerFrame.hlsli"
#include "Bindless.hlsli"

ConstantBuffer<PerFrameData> gPerFrame : register(b2);

struct RefractionDistortionParams {
    float4 distortionTint;       // 屈折領域のカラー色調（重力シアンや電磁マゼンタ）
    float distortionIntensity;   // 屈折の強さ (0.01〜0.20)
    float waveRadius;            // 衝撃波リングの半径 [0.0〜1.0] (ローカルクアッド空間)
    float waveWidth;             // 衝撃波リングの幅
    float chromaticAberration;   // 局所色収差（プリズムRGBズレ）
    uint sceneTextureIndex;      // BindlessシーンカラーSRVインデックス
    float pinchIntensity;        // 中心への引き込み（ブラックホール的ピンチ）強度
    float time;                  // アニメーション時間
    float pad[5];                // 16バイトアライメント (合計64バイト)
};

ConstantBuffer<RefractionDistortionParams> gDistortionParams : register(b6);
SamplerState gSamplerLinear : register(s0);

float4 main(VertexShaderOutput input) : SV_Target {
    // 1. クアッドのローカルUV中心 (0.5, 0.5) からのベクトルと距離
    float2 localDelta = input.texcoord - float2(0.5f, 0.5f);
    float localDist = length(localDelta);
    float2 localDir = (localDist > 0.0001f) ? (localDelta / localDist) : float2(0.0f, 0.0f);

    // 2. クアッドの四角いエッジを完全に隠す円形フェードマスク (0.35〜0.50で滑らかに0へ)
    float quadMask = 1.0f - smoothstep(0.32f, 0.49f, localDist);
    if (quadMask <= 0.001f) {
        discard;
    }

    // 3. 局所屈折オフセットの計算
    // [A] 中心への重力爆縮引き込み（ピンチ歪み）
    float pinchEnvelope = 1.0f - smoothstep(0.0f, 0.42f, localDist);
    pinchEnvelope = pinchEnvelope * pinchEnvelope;
    float2 pinchOffset = localDir * (pinchEnvelope * gDistortionParams.pinchIntensity * gDistortionParams.distortionIntensity);

    // [B] 超音速で外周へ広がる衝撃波リング
    float ringOffset = 0.0f;
    float ringGlow = 0.0f;
    if (gDistortionParams.waveRadius > 0.001f && gDistortionParams.waveWidth > 0.001f) {
        float dRing = abs(localDist - gDistortionParams.waveRadius);
        if (dRing < gDistortionParams.waveWidth) {
            float normD = dRing / gDistortionParams.waveWidth;
            float ringEnv = 1.0f - normD * normD;
            ringEnv = ringEnv * ringEnv;

            // 正弦波の屈折波紋（波頭で強く曲げる）
            ringOffset = sin((localDist - gDistortionParams.waveRadius) / gDistortionParams.waveWidth * 3.14159265f)
                         * ringEnv * gDistortionParams.distortionIntensity * 1.5f;

            // 白熱発光エッジ
            ringGlow = pow(ringEnv, 2.0f) * 1.8f;
        }
    }

    float2 totalOffset = (pinchOffset + localDir * ringOffset) * quadMask;

    // 4. ピクセルのスクリーン座標から背景UVを算出
    float2 screenUV = input.position.xy / gPerFrame.resolution;

    // 5. 局所色収差（プリズムRGBズレ）を伴うシーンカラーサンプリング
    float3 resultScene;
    uint texIdx = gDistortionParams.sceneTextureIndex;

    if (gDistortionParams.chromaticAberration > 0.0001f && length(totalOffset) > 0.0001f) {
        float2 chr = totalOffset * (gDistortionParams.chromaticAberration * 4.0f);
        float r = gTextures[texIdx].Sample(gSamplerLinear, screenUV + totalOffset + chr).r;
        float g = gTextures[texIdx].Sample(gSamplerLinear, screenUV + totalOffset).g;
        float b = gTextures[texIdx].Sample(gSamplerLinear, screenUV + totalOffset - chr).b;
        resultScene = float3(r, g, b);
    } else {
        resultScene = gTextures[texIdx].Sample(gSamplerLinear, screenUV + totalOffset).rgb;
    }

    // 6. カラー色調（重力シアンや電磁マゼンタ）の微光乗算 ＆ 衝撃波リングの発光加算
    float3 tintColor = lerp(float3(1.0f, 1.0f, 1.0f), gDistortionParams.distortionTint.rgb, pinchEnvelope * 0.4f);
    resultScene *= tintColor;
    resultScene += gDistortionParams.distortionTint.rgb * ringGlow;

    return float4(resultScene, quadMask);
}
