/**
 * @file FluidAdvect.CS.hlsl
 * @brief 2D 流体シミュレーション: 半ラグランジュ移流 (Advection)
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

    float2 vel = gInVelocity[coord];
    float2 backUV = uv - vel * gDeltaTime * gInvResolution;

    gOutVelocity[coord] = gInVelocity.SampleLevel(gSamplerLinear, backUV, 0) * gDissipationVel;
    gOutDensity[coord]  = gInDensity.SampleLevel(gSamplerLinear, backUV, 0)  * gDissipationDen;
}
