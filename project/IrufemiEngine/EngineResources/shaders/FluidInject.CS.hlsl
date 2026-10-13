/**
 * @file FluidInject.CS.hlsl
 * @brief 2D 流体シミュレーション: 外力・染料・重力特異点の注入 (Injection)
 */
#include "FluidCommon.hlsli"

Texture2D<float2>   gInVelocity    : register(t0);
Texture2D<float4>   gInDensity     : register(t1);

RWTexture2D<float2> gOutVelocity   : register(u0);
RWTexture2D<float4> gOutDensity    : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 id : SV_DispatchThreadID) {
    if (id.x >= (uint)gResolution.x || id.y >= (uint)gResolution.y) return;
    int2 coord = id.xy;
    float2 uv = (float2(coord) + 0.5) * gInvResolution;

    float2 delta = uv - gInjectPosUV;
    float dist = length(delta);
    float safeRadius = max(0.001, gInjectRadius);
    float factor = exp(-pow(dist / safeRadius, 2.0)); // ガウス分布減衰

    if (gInjectType > 2.5) {
        // 全面ゼロリセット (Clear / Reset)
        gOutVelocity[coord] = float2(0.0, 0.0);
        gOutDensity[coord]  = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    if (gInjectType < 0.5) {
        // 通常の外力注入 (速度ベクトル)
        gOutVelocity[coord] = gInVelocity[coord] + gInjectVelocity * factor * gDeltaTime * 60.0;
        gOutDensity[coord]  = gInDensity[coord];
    } else if (gInjectType < 1.5) {
        // 染料・星雲エネルギーの注入
        gOutVelocity[coord] = gInVelocity[coord];
        gOutDensity[coord]  = gInDensity[coord] + gInjectColor * factor;
    } else {
        // 重力特異点 (引力 + 回転スワール)
        float2 dirToCenter = (dist > 0.0001) ? normalize(delta) : float2(0, 0);
        float2 tangentDir = float2(-dirToCenter.y, dirToCenter.x);

        // 特異点中心核（台風の目）での速度激突・格子ノイズを防ぐため、
        // 中心極近傍（dist < 0.045）を滑らかにゼロへ減衰（ランキン渦モデル）
        float coreDamp = smoothstep(0.005, 0.045, dist);
        float singularityFactor = factor * coreDamp;

        float2 pullForce = -dirToCenter * gPullStrength * factor; // 引力は中心へ素直に吸い込む
        float2 swirlForce = tangentDir * gVortexStrength * singularityFactor; // 回転は中心核でゼロに収束
        float2 totalForce = (pullForce + swirlForce) * gDeltaTime * 60.0;

        gOutVelocity[coord] = gInVelocity[coord] + totalForce;
        gOutDensity[coord]  = gInDensity[coord] + gInjectColor * factor * 0.5;
    }
}
